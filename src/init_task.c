#include <kernel.h>
#include "init_task.h"
#include "io.h"
#include "util.h"

void child(void) {
    int tid_v = tid();
    int parent_tid_v = parent_tid();

    printf("Child (tid=%d, parent_tid=%d)" EOL, tid_v, parent_tid_v);
    io_flush(COM2);

    pass();

    printf("Child (tid=%d, parent_tid=%d)" EOL, tid_v, parent_tid_v);
    io_flush(COM2);
}

const int init_task_priority = 15;

void init_task(void) {
    int i;
    // create two tasks at a lower priority
    for (i = 0; i < 2; i++) {
        int tid = create(16, &child);
        printf("Created: %d" EOL, tid);
        io_flush(COM2);
    }

    // create two tasks at a higher priority
    for (i = 0; i < 2; i++) {
        int tid = create(14, &child);
        printf("Created: %d" EOL, tid);
        io_flush(COM2);
    }

    printf("First: exiting" EOL);
    io_flush(COM2);
}
