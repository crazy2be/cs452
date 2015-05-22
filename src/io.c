#include "io.h"
#include "rbuf.h"

#include "util.h"
#include "drivers/uart.h"
#include "drivers/timer.h"

static struct RBuf rbuf[4];
static long long send_next_train_byte_at = 0;

static inline struct RBuf* output_buffer(int channel) {
    return &rbuf[output_buffer_number(channel)];
}

static inline struct RBuf* input_buffer(int channel) {
    return &rbuf[input_buffer_number(channel)];
}

void io_init() {
	rbuf_init(&rbuf[0]);
	rbuf_init(&rbuf[1]);
	rbuf_init(&rbuf[2]);
	rbuf_init(&rbuf[3]);
	send_next_train_byte_at = timer_time() + TIME_SECOND;
}

void io_putc(int channel, char c) {
	rbuf_put(output_buffer(channel), c);
}

void io_puts(int channel, const char* s) {
	while (*s) { io_putc(channel, *s); s++; }
}

void io_putll(int channel, long long n) {
	io_putc(channel, 'n');
	if (n < 0) {
		io_putc(channel, '-');
		n *= -1;
	}
	long long t = n, d = 1;
	while (t) {
		t /= 10;
		d *= 10;
	}
	d /= 10;
	while (d) {
		io_putc(channel, ((n/d) % 10) + '0');
		d /= 10;
	}
	io_putc(channel, 'N');
}

void io_flush(int channel) {
    struct RBuf *buf = output_buffer(channel);
	while (buf->l > 0) {
		while (!uart_canwrite(channel)) {}
		uart_write(channel, rbuf_take(buf));
	}
}

int io_buf_is_empty(int channel) {
	return output_buffer(channel)->l <= 0;
}

int io_hasc(int channel) {
	return input_buffer(channel)->l > 0;
}

char io_getc(int channel) {
	return rbuf_take(input_buffer(channel));
}

int io_buflen(int bufn) {
	assert(bufn >= 0 && bufn < 4, "invalid bufn");
	return rbuf[bufn].l;
}

void io_service(void) {
	if (rbuf[0].l > 0 && uart_canwrite(0)) {
		long long t = timer_time();
		if (t >= send_next_train_byte_at) {
			char c = rbuf_take(&rbuf[0]);
			if (c != 0xFF) {
				uart_write(0, c);
				send_next_train_byte_at = t + TIME_SECOND/5;
			}
		}
	}
	if (rbuf[1].l > 0 && uart_canwrite(1)) uart_write(1, rbuf_take(&rbuf[1]));
	if (uart_canread(0)) rbuf_put(&rbuf[2], uart_read(0));
	if (uart_canread(1)) rbuf_put(&rbuf[3], uart_read(1));
}
