#include <kernel.h>
#include "init_task.h"
#include "io.h"

void test3(void) {
    io_puts(COM2, "Inside test function 3\n\r");
    io_flush(COM2);
}

const int init_task_priority = 0;

void init_task(void) {
    int i;
    for (i = 1; i <= 10; i++) {
        io_puts(COM2, "Inside test function 2\n\r");
        int t = create(5, &test3);
        io_puts(COM2, "Spawned task: ");
        io_putll(COM2, t);
        io_puts(COM2, "\n\r");
        io_flush(COM2);
        pass();
    }
}
