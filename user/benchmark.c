#include "benchmark.h"
#include <kernel.h>
#include <io.h>
#include "../kernel/drivers/timer.h"

#ifdef SEND_FIRST
#define SEND_PRIORITY 14
#define RECV_PRIORITY 15
#else
#define SEND_PRIORITY 15
#define RECV_PRIORITY 14
#endif

#define MSG_SIZE 64

#define RECEIVER_TID 2
#define ITERATIONS 200000


static void sender(void) {
    unsigned char buf[MSG_SIZE];
    for (unsigned i = 0; i < ITERATIONS; i++) {
        send(RECEIVER_TID, buf, MSG_SIZE, buf, MSG_SIZE);
    }
}

static void receiver(void) {
    int tid;
    unsigned char buf[MSG_SIZE];
    for (unsigned i = 0; i < ITERATIONS; i++) {
        receive(&tid, buf, MSG_SIZE);
        reply(tid, buf, MSG_SIZE);
    }
    send(parent_tid(), 0, 0, buf, MSG_SIZE);
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
        (unsigned) (end - start), MSG_SIZE, ITERATIONS, SEND_PRIORITY - RECV_PRIORITY);
}
