#include <kernel.h>
#include <io.h>

void child(void) {
    int tid_v = tid();
    int parent_tid_v = parent_tid();

    io_printf(COM2, "Child (tid=%d, parent_tid=%d)" EOL, tid_v, parent_tid_v);
    io_flush(COM2);

    pass();

    io_printf(COM2, "Child (tid=%d, parent_tid=%d)" EOL, tid_v, parent_tid_v);
    io_flush(COM2);
}

void init_task(void) {
    int i;
    // create two tasks at a lower priority
    for (i = 0; i < 2; i++) {
        int tid = create(16, &child);
        (void) tid;
        io_printf(COM2, "Created: %d" EOL, tid);
        io_flush(COM2);
    }

    // create two tasks at a higher priority
    for (i = 0; i < 2; i++) {
        int tid = create(14, &child);
        (void) tid;
        io_printf(COM2, "Created: %d" EOL, tid);
        io_flush(COM2);
    }

    io_printf(COM2, "First: exiting" EOL);
    io_flush(COM2);
}

int main(int argc, char *argv[]) {
    boot(init_task, 15);
}
