#include "../lib/min_heap.h"
#include "min_heap.h"
#include <assert.h>

void min_heap_valid(struct min_heap *h) {
	int i, child, offset;
	for (i = 0; i < h->size; i++) {
		child = i * 2 + 1;
		for (offset = 0; offset < 2; offset++) {
			ASSERT(child + offset >= h->size || h->buf[i].key <= h->buf[child + offset].key);
		}
	}
}

int min_heap_tests(void) {
	int i, v, last_priority;
	struct min_heap h;
	int prio[MIN_HEAP_SIZE];

	min_heap_init(&h);

	for (i = 0; i < MIN_HEAP_SIZE; i++) {
		prio[i] = rand();
		min_heap_push(&h, prio[i], i);
		ASSERT(h.size == i + 1);
		min_heap_valid(&h);
	}

	/* min_heap_print(&h); */

	v = min_heap_pop(&h);
	ASSERT(h.size == i - 1);
	min_heap_valid(&h);
	i--;

	last_priority = prio[v];

	for (; i > 0; i--) {
		v = min_heap_pop(&h);
		ASSERT(h.size == i - 1);
		min_heap_valid(&h);

		ASSERT(last_priority <= prio[v]);
		last_priority = prio[v];
	}
	return 0;
}
