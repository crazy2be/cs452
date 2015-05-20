#include <kernel.h>
#include "init_task.h"
#include "io.h"
#include "util.h"

void child(void) {
    printf("Child task %d\n", tid());
    io_flush(COM2);
}

const int init_task_priority = PRIORITY_MAX;

void init_task(void) {
    printf("init task priority is %d\n", init_task_priority);
    io_flush(COM2);
    int i;
    for (i = 0; i < 10; i++) {
        // create tasks in descending priority order
        create(PRIORITY_MIN - i, &child);
    }
}
