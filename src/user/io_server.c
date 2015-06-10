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
#include <hashtable.h>

#define IO_TX 0
#define IO_RX 1
#define IO_TX_NTFY 2
#define IO_RX_NTFY 3
#define IO_STOP 4
#define IO_INFO 5

#define MAX_STR_LEN 256

struct io_request {
	unsigned char type;
	union {
		// buffer for tx & rx_ntfy requests
		// we want this word-aligned, so we can do efficient memcpy to it
		char buf[MAX_STR_LEN];

		// len for rx requests; right now this is always 0
		int len;

		// tx_ntfy & quit requests don't pass any additional data
	} u;
};

// message definitions:
// TX: req: io_request, type followed by n bytes of input
//     resp: unsigned, 0
// RX: req: io_request, type followed by int len
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

static int transmit(int notifier_tid, struct char_rbuf *buf) {
	// we do a little bit of nastiness to pull the message directly out
	// of the rbuf
	char *msg_start = &buf->buf[buf->i];
	// let the msg go either to the end of the rbuf, or to the max size of the notifier buf
	int msg_len = MIN(sizeof(buf->buf) - buf->i, buf->l);
	msg_len = MIN(TX_BUFSZ, msg_len);

	/* printf("buffering out (%d, %d, %d, %d): \"", sizeof(buf->buf) - buf->i, buf->l, TX_BUFSZ, msg_len); */

	reply(notifier_tid, msg_start, msg_len);
	char_rbuf_drop(buf, msg_len);
	return msg_len;
}

static int receive_data(int tid, struct char_rbuf *buf, int len) {
	char reply_buf[MAX_STR_LEN];
	for (unsigned i = 0; i < len && buf->l > 0; i++) {
		reply_buf[i] = char_rbuf_take(buf);
	}
	reply(tid, reply_buf, len);
	return len;
}

static int notifier_get_channel(int server_tid) {
	int channel, tid;
	receive(&tid, &channel, sizeof(channel));
	reply(tid, NULL, 0);
	return channel;
}

static void tx_notifier(void) {
	int parent = parent_tid();
	const int evt = (notifier_get_channel(parent) * 2) + EID_COM1_WRITE;
	char buf[TX_BUFSZ];
	const char req = IO_TX_NTFY;

	for (;;) {
		int len = send(parent, &req, sizeof(req), buf, TX_BUFSZ);
		ASSERT(len >= 0); // shouldn't have gotten an error
		// if (len <= 0) break; // quit if no work left TODO
		await(evt, buf, len);
	}
}

static void rx_notifier(void) {
	int parent = parent_tid();
	const int evt = (notifier_get_channel(parent) * 2) + EID_COM1_READ;
	char buf[RX_BUFSZ + 1];
	unsigned resp;

	for (;;) {
		await(evt, buf + 1, RX_BUFSZ);
		buf[0] = IO_RX_NTFY;
		send(parent, buf, sizeof(buf), &resp, sizeof(resp));
		// if (resp) break; // quit if no work left TODO
	}
}

static void io_server_run(const int channel, const char *name) {
	register_as(name);

	// buffers to accumulate data
	struct char_rbuf tx_buf, rx_buf;

	// tasks awaiting IO (the one at the front of the queue is currently being
	// serviced)
	struct io_rbuf tx_waiters, rx_waiters;

	const struct io_blocked_task *task;
	struct io_blocked_task temp;

	int bytes_tx = 0, bytes_rx = 0;
	int tx_ntfy = -1;
	unsigned resp;

	for (;;) {
		// invariants:
		// there can't be both the notifier and the client tasks waiting at
		// the same time
		ASSERT(tx_ntfy < 0 || io_rbuf_empty(&tx_waiters));

		struct io_request req;
		int tid;

		int msg_len = receive(&tid, &req, sizeof(req));
		ASSERT(msg_len >= 1);

		switch (req.type) {
		case IO_TX:
			msg_len -= 4; // don't count the initial type in the length
			ASSERT(msg_len > 0); // TODO make this an error message
			printf("Printing msg of %d bytes" EOL, msg_len);
			temp = (struct io_blocked_task) {
				.tid = tid,
				.byte_count = msg_len,
			};
			io_rbuf_put(&tx_waiters, temp);
			for (int i = 0; i < msg_len; i++) {
				char_rbuf_put(&tx_buf, req.u.buf[i]);
			}

			if (tx_ntfy >= 0) {
				bytes_tx += transmit(tx_ntfy, &tx_buf);
				tx_ntfy = -1;
			}
			break;
		case IO_TX_NTFY:
			task = io_rbuf_empty(&tx_waiters) ? NULL : io_rbuf_peek(&tx_waiters);
			// TODO: better control flow here?
			if (task && bytes_tx >= task->byte_count) {
				// the notifier just came back from outputting the last of this task's
				// data, so we can now return that task
				bytes_tx -= task->byte_count;
				resp = 0;
				reply(task->tid, &resp, sizeof(resp));

				// advance to the next task
				io_rbuf_drop(&tx_waiters, 1);
				task = io_rbuf_empty(&tx_waiters) ? NULL : io_rbuf_peek(&tx_waiters);
			}
			if (task) {
				bytes_tx += transmit(tid, &tx_buf);
			} else {
				tx_ntfy = tid;
			}
			break;
		case IO_RX:
			ASSERT(msg_len == 8);
			if (bytes_rx >= req.u.len) {
				bytes_rx -= receive_data(tid, &rx_buf, req.u.len);
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

			msg_len -= 4; // don't count the initial type in the length
			// copy input into buffer
			for (int i = 0; i < msg_len; i++) {
				char_rbuf_put(&rx_buf, req.u.buf[i]);
			}
			bytes_rx += msg_len;

			for (;;) {
				task = io_rbuf_empty(&rx_waiters) ? NULL : io_rbuf_peek(&rx_waiters);
				if (!task || bytes_rx < task->byte_count) break;

				bytes_rx -= receive_data(task->tid, &rx_buf, task->byte_count);
				io_rbuf_drop(&rx_waiters, 1);
			}
			break;
		case IO_STOP:
			// TODO
			break;
		default:
			ASSERT(0 && "Unknown request made to IO server");
			break;
		}
	}
}

struct io_server_init_msg {
	int channel;

	// length bounded by max namelen supported by nameserver
	char name[MAX_KEYLEN + 1];
};

#define NOTIFIER_COUNT 2

// startup routines for io server
static void io_server_init(void) {
	struct io_server_init_msg resp;
	int tid;

	// our parent immediately sends us some bootstrap info
	receive(&tid, &resp, sizeof(resp));
	reply(tid, NULL, 0);

	// start up notifiers, and give them their bootstrap info in turn
	static void (*notifiers[NOTIFIER_COUNT])(void) = { rx_notifier, tx_notifier };

	for (int i = 0; i < NOTIFIER_COUNT; i++) {
		tid = create(1, notifiers[i]);
		ASSERT(tid > 0);
		ASSERT(send(tid, &resp.channel, sizeof(resp.channel), NULL, 0) == 0);
	}

	io_server_run(resp.channel, resp.name);
}

void ioserver(const int priority, const int channel, const char *name) {
	int tid = create(priority, io_server_init);
	struct io_server_init_msg resp;
	resp.channel = channel;
	strcpy(resp.name, name);
	send(tid, &resp, sizeof(int) + strlen(name) + 1, NULL, 0);
}

// eventually, we'll have to support IO servers for COM1 & COM2
static int io_server_tid(int channel) {
	static int tids[2] = {-1 , -1};
	if (tids[channel] < 0) {
		switch (channel) {
		case COM1:
			tids[channel] = whois("com1_io_server");
			break;
		case COM2:
			tids[channel] = whois("com2_io_server");
			break;
		default:
			ASSERT(0 && "No IO server for unknown COM channel");
			break;
		}
	}
	return tids[channel];
}

// functions which interact with the io server
int iosrv_puts(const int channel, const char *str) {
	int len = strlen(str);
	ASSERT(len <= MAX_STR_LEN);
	struct io_request req;
	unsigned resp;

	req.type = IO_TX;
	memcpy(req.u.buf, str, len);

	// TODO: offsetof() ?
	unsigned msg_len = 4 + len; // need to worry about alignment when doing this calculation
	int err = send(io_server_tid(channel), &req, msg_len, &resp, sizeof(resp));
	ASSERT(err >= 0);

	return (err < 0) ? -1 : 0;
}

/* int putc(const char c); */
int iosrv_getc(int channel) {
	struct io_request req;

	req.type = IO_RX;
	req.u.len = 1;

	unsigned msg_len = sizeof(req) - sizeof(req.u.buf) + sizeof(req.u.len);
	char c;
	int err = send(io_server_tid(channel), &req, msg_len, &c, sizeof(c));
	ASSERT(err >= 0);

	return (err < 0) ? -1 : c;
}
