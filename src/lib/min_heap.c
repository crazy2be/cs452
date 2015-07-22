#include "min_heap.h"

#include <assert.h>

void min_heap_init(struct min_heap *mh) {
	mh->size = 0;
}

void min_heap_push(struct min_heap *mh, int key, int value) {
	ASSERT(mh->size < MIN_HEAP_SIZE && "Out of space in min-heap.");

	int i, parent;
	// start at the first empty spot, and try to insert there
	for (i = mh->size; i > 0; i = parent) {
		// compare with the parent - if the parent is of lower key,
		// put the parent in this spot, and try to insert in the parent's spot

		parent = (i - 1) / 2;
		if (mh->buf[parent].key <= key) break;
		mh->buf[i] = mh->buf[parent];
	}

	mh->buf[i].key = key;
	mh->buf[i].value = value;
	mh->size++;
}

int min_heap_empty(struct min_heap *mh) {
	return mh->size == 0;
}

int min_heap_top_key(struct min_heap *mh) {
	ASSERT(mh->size > 0 && "Heap is topless.");
	return mh->buf[0].key;
}

int min_heap_pop(struct min_heap *mh) {
	ASSERT(mh->size > 0 && "Nothing left in the min-heap!");

	// get the value
	int value = mh->buf[0].value;

	// peel off the last value, and insert it at the top of the heap,
	// maintaining heap order
	struct min_heap_node e = mh->buf[--mh->size];

	int i, child;
	for (i = 0; ; i = child) {
		child = i * 2 + 1;
		if (child >= mh->size) break;

		if (child + 1 < mh->size &&
		        mh->buf[child].key > mh->buf[child+1].key) {
			child++;
		}

		if (mh->buf[child].key >= e.key) break;
		mh->buf[i] = mh->buf[child];
	}

	mh->buf[i] = e;

	return value;
}
