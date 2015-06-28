#include "io_server.h"

#include "../user/nameserver.h"
#include "../kernel/drivers/uart.h"
#include "../kernel/kassert.h"
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
#define IO_DUMP 5
#define IO_RXBUFLEN 6

#define MAX_STR_LEN 256

#define COM1_SRV_NAME "com1_iosrv"
#define COM2_SRV_NAME "com2_iosrv"

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
// INFO: req: io_request, just type
//       resp: int com number


// TODO: this can be larger if we change await to return if
// there's some input, instead of when the buffer fills up
#define RX_BUFSZ 1
#define TX_BUFSZ 16

static void transmit(int notifier_tid, struct char_rbuf *buf) {
	// we do a little bit of nastiness to pull the message directly out
	// of the rbuf
	char *msg_start = &buf->buf[buf->i];
	// let the msg go either to the end of the rbuf, or to the max size of the notifier buf
	int msg_len = MIN(sizeof(buf->buf) - buf->i, buf->l);
	msg_len = MIN(TX_BUFSZ, msg_len);

	try_reply(notifier_tid, msg_start, msg_len);
	char_rbuf_drop(buf, msg_len);
}

static int try_receive_data(int tid, struct char_rbuf *buf, int len) {
	char reply_buf[MAX_STR_LEN];
	int i;
	for (i = 0; i < len && buf->l > 0; i++) {
		reply_buf[i] = char_rbuf_take(buf);
	}
	try_reply(tid, reply_buf, i);
	return len;
}

static int notifier_get_channel(int server_tid) {
	int channel, tid;
	try_receive(&tid, &channel, sizeof(channel));
	try_reply(tid, NULL, 0);
	return channel;
}

static void tx_notifier(void) {
	int parent = parent_tid();
	int channel = notifier_get_channel(parent);
	const int evt = (channel* 2) + EID_COM1_WRITE;
	char buf[TX_BUFSZ];
	const char req = IO_TX_NTFY;

	for (;;) {
		int len = try_send(parent, &req, sizeof(req), buf, TX_BUFSZ);
		if (len == 0) continue; // try_await behaves badly if you try_send an empty buffer
		else if (len < 0) break; // quit if the server shut down
		ASSERT(evt == EID_COM1_WRITE || evt == EID_COM2_WRITE);
		try_await(evt, buf, len);
	}
}

static void rx_notifier(void) {
	int parent = parent_tid();
	const int evt = (notifier_get_channel(parent) * 2) + EID_COM1_READ;
	char buf[4 + RX_BUFSZ];
	unsigned resp;

	for (;;) {
		ASSERT(evt == EID_COM1_READ || evt == EID_COM2_READ);
		try_await(evt, buf + 4, RX_BUFSZ);
		buf[0] = IO_RX_NTFY;
		int err = try_send(parent, buf, sizeof(buf), &resp, sizeof(resp));
		if (err < 0) break; // quit if the server shut down
	}
}

static void io_server_run() {
	// buffers to accumulate data
	struct char_rbuf tx_buf, rx_buf;

	// tasks try_awaiting IO (the one at the front of the queue is currently being
	// serviced)
	struct io_rbuf rx_waiters;

	const struct io_blocked_task *task;
	struct io_blocked_task temp;

	int bytes_rx = 0;
	int tx_ntfy = -1;
	unsigned resp;
	int shutdown_tid = -1;

	char_rbuf_init(&tx_buf);
	char_rbuf_init(&rx_buf);
	io_rbuf_init(&rx_waiters);

	for (;;) {
		struct io_request req;
		int tid;

		int msg_len = try_receive(&tid, &req, sizeof(req));
		ASSERT(msg_len >= 1);

		switch (req.type) {
		case IO_TX:
			ASSERT(shutdown_tid < 0 && "Got new TX request while shutting down");
			msg_len -= 4; // don't count the initial type in the length
			ASSERT(msg_len >= 0); // TODO make this an error message

			resp = 0;
			try_reply(tid, &resp, sizeof(resp));

			for (int i = 0; i < msg_len; i++) {
				ASSERT(!char_rbuf_full(&tx_buf));
				char_rbuf_put(&tx_buf, req.u.buf[i]);
			}

			if (tx_ntfy >= 0) {
				transmit(tx_ntfy, &tx_buf);
				tx_ntfy = -1;
			}
			break;
		case IO_TX_NTFY:
			if (!char_rbuf_empty(&tx_buf)) {
				transmit(tid, &tx_buf);
			} else if (shutdown_tid < 0) {
				tx_ntfy = tid;
			} else {
				goto cleanup;
			}
			break;
		case IO_RX:
			ASSERT(shutdown_tid < 0 && "Got new RX request while shutting down");
			ASSERT(msg_len == 8);
			if (bytes_rx >= req.u.len) {
				bytes_rx -= try_receive_data(tid, &rx_buf, req.u.len);
			} else {
				temp = (struct io_blocked_task) {
					.tid = tid,
					.byte_count = req.u.len,
				};
				io_rbuf_put(&rx_waiters, temp);
			}
			break;
		case IO_RX_NTFY:
			if (shutdown_tid < 0) {
				// immediately try_reply to notifier to get more input
				resp = 0;
				try_reply(tid, &resp, sizeof(resp));
			} else {
				// we're shutting down - just drop the whole thing on the floor
				// nobody should be listening for this input anyway
				break;
			}

			msg_len -= 4; // don't count the initial type in the length
			// copy input into buffer
			for (int i = 0; i < msg_len; i++) {
				ASSERT(!char_rbuf_full(&rx_buf));
				char_rbuf_put(&rx_buf, req.u.buf[i]);
			}

			bytes_rx += msg_len;

			for (;;) {
				task = io_rbuf_empty(&rx_waiters) ? NULL : io_rbuf_peek(&rx_waiters);
				if (!task || bytes_rx < task->byte_count) break;

				bytes_rx -= try_receive_data(task->tid, &rx_buf, task->byte_count);
				io_rbuf_drop(&rx_waiters, 1);
			}
			break;
		case IO_STOP:
			ASSERT(shutdown_tid < 0 && "Got new shutdown request while shutting down");
			shutdown_tid = tid;
			if (char_rbuf_empty(&tx_buf)) goto cleanup;
			// we now need to wait for all characters of output to be flushed
			break;
		case IO_DUMP:
			try_reply(tid, &rx_buf.l, sizeof(rx_buf.l));
			char_rbuf_init(&rx_buf);
			break;
		case IO_RXBUFLEN:
			try_reply(tid, &rx_buf.l, sizeof(rx_buf.l));
			break;
		default:
			ASSERT(0 && "Unknown request made to IO server");
			break;
		}
	}

cleanup:
	ASSERT(char_rbuf_empty(&tx_buf));
	try_reply(shutdown_tid, NULL, 0);
}

#define NOTIFIER_COUNT 2

// startup routines for io server
static void io_server_init(void) {
	int channel;
	int tid;

	// our parent immediately try_sends us some bootstrap info
	try_receive(&tid, &channel, sizeof(channel));
	register_as((channel == COM1) ? COM1_SRV_NAME : COM2_SRV_NAME);
	try_reply(tid, NULL, 0);

	// start up notifiers, and give them their bootstrap info in turn
	static void (*notifiers[NOTIFIER_COUNT])(void) = { rx_notifier, tx_notifier };

	for (int i = 0; i < NOTIFIER_COUNT; i++) {
		tid = try_create(LOWER(PRIORITY_MAX, 2), notifiers[i]);
		ASSERT(tid > 0);
		ASSERT(try_send(tid, &channel, sizeof(channel), NULL, 0) == 0);
	}

	io_server_run();
}

void ioserver(const int priority, const int channel) {
	int tid = try_create(priority, io_server_init);
	try_send(tid, &channel, sizeof(channel), NULL, 0);
}

//
// All of the code below is called by clients to the IO server
//

static int io_server_tid(int channel) {
	static int tids[2] = {-1 , -1};
	if (tids[channel] < 0) {
		switch (channel) {
		case COM1:
			tids[channel] = whois(COM1_SRV_NAME);
			break;
		case COM2:
			tids[channel] = whois(COM2_SRV_NAME);
			break;
		default:
			ASSERT(0 && "No IO server for unknown COM channel");
			break;
		}
	}
	return tids[channel];
}


// bw analogues to the functions below
int bw_putc(const char c, const int channel) {
	/* KASSERT(!usermode()); */
	while(!uart_canwritefifo(channel));
	uart_write(channel, c);
	return 0;
}

int bw_puts(const char *str, const int channel) {
	/* KASSERT(!usermode()); */
	while (*str) bw_putc(*str++, channel);
	return 0;
}

int bw_gets(char *buf, int len, const int channel) {
	/* KASSERT(!usermode()); */
	while (len-- > 0) {
		while (!uart_canread(channel));
		*buf++ = uart_read(channel);
	}
	return 0;
}

int fput_buf(const char *buf, int buflen, const int channel) {
	struct io_request req;
	unsigned resp;

	req.type = IO_TX;
	memcpy(req.u.buf, buf, buflen);

	// TODO: offsetof() ?
	unsigned msg_len = 4 + buflen; // need to worry about alignment when doing this calculation
	int err = try_send(io_server_tid(channel), &req, msg_len, &resp, sizeof(resp));
	ASSERT(err >= 0);

	return (err < 0) ? -1 : 0;
}
// functions which interact with the io server
int fputs(const char *str, const int channel) {
	if (channel == COM2_DEBUG) return bw_puts(str, COM2);

	if (!usermode()) {
		KASSERT(0);
	}
	int len = strlen(str);
	ASSERT(len <= MAX_STR_LEN);
	return fput_buf(str, len, channel);
}

int fputc(const char c, const int channel) {
	if (channel == COM2_DEBUG) return bw_putc(c, COM2);

	KASSERT(usermode());
	return fput_buf(&c, 1, channel);
}

int fgets(char *buf, int len, const int channel) {
	if (channel == COM2_DEBUG) return bw_gets(buf, len, COM2);

	KASSERT(usermode());
	struct io_request req;

	req.type = IO_RX;
	req.u.len = len;

	unsigned msg_len = sizeof(req) - sizeof(req.u.buf) + sizeof(req.u.len);
	int err = try_send(io_server_tid(channel), &req, msg_len, buf, len);
	ASSERT(err >= 0);

	return err;
}

int fgetc(int channel) {
	char c;
	int err = fgets(&c, 1, channel);
	return (err < 0) ? err : c;
}

int fbuflen(const int channel) {
	struct io_request req;
	req.type = IO_RXBUFLEN;
	unsigned msg_len = sizeof(req) - sizeof(req.u.buf) + sizeof(req.u.len);
	int resp;
	int err = try_send(io_server_tid(channel), &req, msg_len, &resp, sizeof(resp));
	ASSERT(err >= 0);
	return resp;
}

int fdump(const int channel) {
	struct io_request req;
	req.type = IO_DUMP;
	unsigned msg_len = sizeof(req) - sizeof(req.u.buf) + sizeof(req.u.len);
	int resp;
	int err = try_send(io_server_tid(channel), &req, msg_len, &resp, sizeof(resp));
	ASSERT(err >= 0);
	return err;
}

// blocks until all output in the buffers is flushed, and the server is shutting down
void ioserver_stop(const int channel) {
	unsigned char msg = IO_STOP;
	try_send(io_server_tid(channel), &msg, sizeof(msg), NULL, 0);
}
