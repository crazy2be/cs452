#include "benchmark.h"
#include <kernel.h>
#include <io.h>
#include <assert.h>
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

#define RECEIVER_TID 2
#define ITERATIONS 2000


static void sender(void) {
    unsigned char send_buf[BENCHMARK_MSG_SIZE];
    unsigned char recv_buf[BENCHMARK_MSG_SIZE];
    for (unsigned j = 0; j < BENCHMARK_MSG_SIZE; j++) {
        send_buf[j] = 0xcd;
    }
    for (unsigned i = 0; i < ITERATIONS; i++) {
        send(RECEIVER_TID, send_buf, BENCHMARK_MSG_SIZE, recv_buf, BENCHMARK_MSG_SIZE);
        /* for (unsigned j = 0; j < BENCHMARK_MSG_SIZE; j++) { */
        /*     ASSERT(recv_buf[j] == 0xab); */
        /* } */
    }
}

static void receiver(void) {
    int tid;
    unsigned char recv_buf[BENCHMARK_MSG_SIZE];
    unsigned char repl_buf[BENCHMARK_MSG_SIZE];
    for (unsigned j = 0; j < BENCHMARK_MSG_SIZE; j++) {
        repl_buf[j] = 0xab;
    }
    for (unsigned i = 0; i < ITERATIONS; i++) {
        receive(&tid, recv_buf, BENCHMARK_MSG_SIZE);
        /* for (unsigned j = 0; j < BENCHMARK_MSG_SIZE; j++) { */
        /*     ASSERT(recv_buf[j] == 0xcd); */
        /* } */
        reply(tid, repl_buf, BENCHMARK_MSG_SIZE);
    }
    send(parent_tid(), 0, 0, recv_buf, BENCHMARK_MSG_SIZE);
}

void benchmark(void) {
    unsigned dummy;
    int tid;

    unsigned start = 0xffffffff - timer_time();

    create(SEND_PRIORITY, sender);
    create(RECV_PRIORITY, receiver);
    receive(&tid, &dummy, sizeof(dummy));

    unsigned end = 0xffffffff - timer_time();

    printf("Benchmark took %d ns (msg_size = %d, iterations = %d, pdelta = %d)" EOL,
        (unsigned) (end - start) * 1000 / ITERATIONS, BENCHMARK_MSG_SIZE, ITERATIONS, SEND_PRIORITY - RECV_PRIORITY);
}
