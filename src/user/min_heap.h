// neither of these should be directly manipulated by
// callers - they are only here so callers know how to allocate
// space for a priority min_heap

// YOLO: This could be renamed to more natually group as min-heap SIZE not
// min heap-size.
#include <util.h>

#define MIN_HEAP_SIZE 256
#define MIN_HEAP_KEY int
#ifndef MIN_HEAP_VALUE
#error "No MIN_HEAP_VALUE defined!"
#endif
#ifndef MIN_HEAP_PREFIX
#error "No MIN_HEAP_PREFIX defined!"
#endif

#define MH_T struct PASTER(MIN_HEAP_PREFIX, min_heap)
#define MH_NODE_T struct PASTER(MIN_HEAP_PREFIX, min_heap_node)
#define MH_INIT PASTER(MIN_HEAP_PREFIX, min_heap_init)
#define MH_EMPTY PASTER(MIN_HEAP_PREFIX, min_heap_empty)
#define MH_TOP_KEY PASTER(MIN_HEAP_PREFIX, min_heap_top_key)
#define MH_PUSH PASTER(MIN_HEAP_PREFIX, min_heap_push)
#define MH_POP PASTER(MIN_HEAP_PREFIX, min_heap_pop)

MH_NODE_T {
	MIN_HEAP_KEY key;
	MIN_HEAP_VALUE value;
};

MH_T {
	MH_NODE_T buf[MIN_HEAP_SIZE];
	int size;
};


// actual definitions

#include <assert.h>

static void MH_INIT(MH_T *mh) {
	mh->size = 0;
}

static int MH_EMPTY(MH_T *mh) {
	return mh->size == 0;
}

// push a value into the min_heap with the given prority
static void MH_PUSH(MH_T *mh, MIN_HEAP_KEY key, MIN_HEAP_VALUE value) {
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

MIN_HEAP_KEY MH_TOP_KEY(MH_T *mh) {
	ASSERT(mh->size > 0 && "Heap is topless.");
	return mh->buf[0].key;
}

// if the min_heap is non-empty, pop the highest priority element out of the min_heap
// the value is written to the given pointer, and 0 is returned.
// else, 1 is returned
MIN_HEAP_VALUE MH_POP(MH_T *mh) {
	ASSERT(mh->size > 0 && "Nothing left in the min-heap!");

	// get the value
	MIN_HEAP_VALUE value = mh->buf[0].value;

	// peel off the last value, and insert it at the top of the heap,
	// maintaining heap order
	MH_NODE_T e = mh->buf[--mh->size];

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
