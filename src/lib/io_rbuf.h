#pragma once

struct io_blocked_task {
	int tid;
	unsigned byte_count;
};

#define RBUF_SIZE 256
#define RBUF_ELE struct io_blocked_task
#define RBUF_PREFIX io
#include "rbuf_generic.h"
