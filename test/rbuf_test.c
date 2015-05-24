#if 0
#include "rbuf.h"

#include <assert.h>
#include <stdio.h>

// Make sure there are no off-by-one errors...
struct WB {
	int canary_before;
	struct RBuf rbuf;
	int canary_after;
} __attribute__((packed));

static void wb_init(struct WB *wb) {
	wb->canary_before = 0xF7F7F7F7;
	wb->canary_after = 0x7F7F7F7F;
	rbuf_init(&wb->rbuf);
}

static void wb_verify(struct WB *wb) {
	assert(wb->canary_before == 0xF7F7F7F7);
	assert(wb->canary_after == 0x7F7F7F7F);
}

static void init() {
	printf("initialization test...\n");
	struct WB wb;
	wb_init(&wb);
	for (int i = 0; i < wb.rbuf.c; i++) {
		assert(wb.rbuf.buf[i] == 0);
	}
	wb_verify(&wb);
	printf("done!\n");
}

static void simple() {
	printf("simple test...\n");
	struct WB wb;
	wb_init(&wb);
	rbuf_put(&wb.rbuf, 10);
	assert(rbuf_take(&wb.rbuf) == 10);
	wb_verify(&wb);
	printf("done!\n");
}

static void full() {
	printf("full test...\n");
	struct WB wb;
	wb_init(&wb);
	for (int i = 0; i < sizeof(wb.rbuf.buf); i++) {
		rbuf_put(&wb.rbuf, i);
	}
	for (int i = 0; i < sizeof(wb.rbuf.buf); i++) {
		char val = rbuf_take(&wb.rbuf);
		assert(val == (char)i);
	}
	wb_verify(&wb);
	printf("done!\n");
}

static void wrap() {
	printf("wrap test...\n");
	struct WB wb;
	wb_init(&wb);
	for (int r = 0; r < 10; r++) {
		for (int i = 0; i < sizeof(wb.rbuf.buf); i++) {
			rbuf_put(&wb.rbuf, i);
		}
		for (int i = 0; i < sizeof(wb.rbuf.buf); i++) {
			char val = rbuf_take(&wb.rbuf);
			assert(val == (char)i);
		}
		printf("done round %d...\n", r + 1);
	}
	wb_verify(&wb);
	printf("done!\n");
}

void main() {
	printf("testing ring buffer...\n");
	init();
	simple();
	full();
	wrap();
}
#endif
