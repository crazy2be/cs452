#include "io_server.h"

#include "nameserver.h"
#include <io_rbuf.h>
#undef RBUF_SIZE
#undef RBUF_ELE
#undef RBUF_PREFIX
#include <rbuf.h>
#include <kernel.h>
#include <util.h>
#include <assert.h>

#define IO_TX 0
#define IO_RX 1
#define IO_TX_NTFY 2
#define IO_RX_NTFY 3
#define IO_STOP 4
#define IO_INFO 5

#define MAX_STR_LEN 255

struct io_request {
	unsigned char type;
	union {
		char buf[MAX_STR_LEN]; // buffer for tx & rx_ntfy requests
		unsigned len; // len for rx requests; right now this is always 0
		// tx_ntfy & quit requests don't pass any additional data
	} u;
};

// message definitions:
// TX: req: io_request, type followed by n bytes of input
//     resp: unsigned, 0
// RX: req: io_request, type followed by unsigned len
//     resp: char buffer of bytes returned
// TX_NTFY: req: io_request, just type
//          resp: char buffer of bytes to send
// RX_NTFY: req: io_request, type followed by n bytes of input
//          resp: unsigned 0
// STOP:
// INFO: req: io_request, just type
//       resp: int com number


// TODO: this can be larger if we change await to return if
// there's some input, instead of when the buffer fills up
#define RX_BUFSZ 1
#define TX_BUFSZ 16

static unsigned transmit(int notifier_tid, struct char_rbuf *buf) {
	// we do a little bit of nastiness to pull the message directly out
	// of the rbuf
	char *msg_start = &buf->buf[buf->i];
	// let the msg go either to the end of the rbuf, or to the max size of the notifier buf
	unsigned msg_len = MIN(TX_BUFSZ, sizeof(buf->buf) - buf->i);
	reply(notifier_tid, msg_start, msg_len);
	char_rbuf_drop(buf, msg_len);
	return msg_len;
}

int notifier_get_channel(int server_tid) {
	char req = IO_INFO;
	int channel;
	send(parent, buf, 1, &channel, sizeof(channel));
	return channel;
}

void tx_notifier(void) {
	int parent = parent_tid();
	const int evt = (notifier_get_channel(parent) * 2) + EVT_COM1_WRITE;
	char buf[TX_BUFSZ];
	const char req = IO_TX_NTFY;

	for (;;) {
		int len = send(parent, req, sizeof(req), buf, TX_BUFSZ);
		ASSERT(len >= 0); // shouldn't have gotten an error
		// if (len <= 0) break; // quit if no work left TODO
		await(evt, buf, TX_BUFSZ);
	}
}

void rx_notifier(void) {
	int parent = parent_tid();
	const int evt = (notifier_get_channel(parent) * 2) + EVT_COM1_READ;
	char buf[RX_BUFSZ + 1];
	unsigned resp;

	for (;;) {
		await(evt, buf + 1, RX_BUFSZ);
		buf[0] = IO_RX_NOTIFY;
		send(parent, buf, sizeof(buf), resp, sizeof(resp));
	}
}

void io_server_start(const int channel, const char *name) {
	register_as(name);

	// buffers to accumulate data
	struct char_rbuf tx_buf, rx_buf;

	// tasks awaiting IO (the one at the front of the queue is currently being
	// serviced)
	struct io_rbuf tx_waiters, rx_waiters;

	const struct io_blocked_task *task;
	struct io_blocked_task temp;

	unsigned bytes_tx = 0, bytes_rx = 0, resp;
	int tx_ntfy = -1, rx_ntfy = -1;

	create(1, rx_notifier);
	create(1, tx_notifier);

	for (;;) {
		// invariants:
		// there can't be both the notifier and the client tasks waiting at
		// the same time
		ASSERT(tx_ntfy < 0 || tx_waiters.l == 0);
		ASSERT(rx_ntfy < 0 || rx_waiters.l == 0);

		struct io_request req;
		int tid;

		int msg_len = receive(&tid, &req, sizeof(req)) - 1;
		ASSERT(msg_len > 0);

		switch (req.type) {
		case IO_TX:
			temp = (struct io_blocked_task) {
				.tid = tid,
				.byte_count = msg_len,
			};
			io_rbuf_put(&tx_waiters, temp);
			for (int i = 0; i < msg_len; i++) {
				char_rbuf_put(&tx_buf, req.u.buf[i]);
			}
			if (tx_ntfy > 0) {
				bytes_tx += transmit(tx_ntfy, &tx_buf);
				tx_ntfy = -1;
			}
			break;
		case IO_TX_NTFY:
			task = io_rbuf_peek(&tx_waiters);
			// TODO: better control flow here?
			if (task && bytes_tx >= task->byte_count) {
				// the notifier just came back from outputting the last of this task's
				// data, so we can now return that task
				bytes_tx -= task->byte_count;
				resp = 0;
				reply(task->tid, &resp, sizeof(resp));

				// advance to the next task
				io_rbuf_drop(&tx_waiters, 1);
				task = io_rbuf_peek(&tx_waiters);
			}
			if (task) {
				transmit(tid, &tx_buf);
			} else {
				tx_ntfy = tid;
			}
			break;
		case IO_RX:
			if (bytes_rx >= req.u.len) {
				unsigned len = req.u.len;
				bytes_rx -= len;
				for (unsigned i = 0; i < len; i++) {
					req.u.buf[i] = char_rbuf_take(&rx_buf);
				}
				reply(tid, req.u.buf, len);
			} else {
				temp = (struct io_blocked_task) {
					.tid = tid,
					.byte_count = req.u.len,
				};
				io_rbuf_put(&rx_waiters, temp);
			}
			break;
		case IO_RX_NTFY:
			// immediately reply to notifier to get more input
			resp = 0;
			reply(tid, &resp, sizeof(resp));

			// copy input into buffer
			for (int i = 0; i < msg_len; i++) {
				char_rbuf_put(&rx_buf, req.u.buf[i]);
			}
			bytes_rx += msg_len;

			task = io_rbuf_peek(&rx_waiters);
			while (task && bytes_rx >= task->byte_count) {
				bytes_rx -= task->byte_count;
				for (unsigned i = 0; i < task->byte_count; i++) {
					req.u.buf[i] = char_rbuf_take(&rx_buf);
				}
				reply(task->tid, req.u.buf, task->byte_count);
				io_rbuf_drop(&rx_waiters, 1);
				task = io_rbuf_peek(&rx_waiters);
			}
			break;
		case IO_QUIT:
			// TODO
			break;
		case IO_INFO:
			resp = channel;
			reply(tid, &resp, sizeof(resp));
			break;
		default:
			ASSERT(0 && "Unknown request made to IO server");
			break;
		}
	}
}
