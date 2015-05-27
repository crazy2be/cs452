#include "benchmark.h"
#include <kernel.h>
#include <io.h>
#include "../kernel/drivers/timer.h"

// we expect the build script to provide BENCHMARK_SEND_FIRST, BENCHMARK_CACHE,
// and BENCHMARK_MSG_SIZE

#if BENCHMARK_SEND_FIRST
#define SEND_PRIORITY 14
#define RECV_PRIORITY 15
#else
#define SEND_PRIORITY 15
#define RECV_PRIORITY 14
#endif

#define BENCHMARK_MSG_SIZE 64

#define RECEIVER_TID 2
#define ITERATIONS 200


static void sender(void) {
    unsigned char buf[BENCHMARK_MSG_SIZE];
    for (unsigned i = 0; i < ITERATIONS; i++) {
        send(RECEIVER_TID, buf, BENCHMARK_MSG_SIZE, buf, BENCHMARK_MSG_SIZE);
    }
}

static void receiver(void) {
    int tid;
    unsigned char buf[BENCHMARK_MSG_SIZE];
    for (unsigned i = 0; i < ITERATIONS; i++) {
        receive(&tid, buf, BENCHMARK_MSG_SIZE);
        reply(tid, buf, BENCHMARK_MSG_SIZE);
    }
    send(parent_tid(), 0, 0, buf, BENCHMARK_MSG_SIZE);
}

void benchmark(void) {
    unsigned dummy;
    int tid;

    long long start = timer_time();

    create(SEND_PRIORITY, sender);
    create(RECV_PRIORITY, receiver);
    receive(&tid, &dummy, sizeof(dummy));

    long long end = timer_time();

    printf("Benchmark took %d time (msg_size = %d, iterations = %d, pdelta = %d)" EOL,
        (unsigned) (end - start), BENCHMARK_MSG_SIZE, ITERATIONS, SEND_PRIORITY - RECV_PRIORITY);
}
