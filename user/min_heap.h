#pragma once
// neither of these should be directly manipulated by
// callers - they are only here so callers know how to allocate
// space for a priority min_heap

// YOLO: This could be renamed to more natually group as min-heap SIZE not
// min heap-size.
#define MIN_HEAP_SIZE 256

struct min_heap_node {
	int key;
	int value;
};

struct min_heap {
	struct min_heap_node buf[MIN_HEAP_SIZE];
	int size;
};

void min_heap_init(struct min_heap *q);

int min_heap_empty(struct min_heap *mh);

// push a value into the min_heap with the given prority
void min_heap_push(struct min_heap *q, int key, int value);


int min_heap_top_key(struct min_heap *q);

// if the min_heap is non-empty, pop the highest priority element out of the min_heap
// the value is written to the given pointer, and 0 is returned.
// else, 1 is returned
int min_heap_pop(struct min_heap *q);
