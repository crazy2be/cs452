#include <kernel.h>
#include <bwio.h>

void child(void) {
    int tid_v = tid();
    int parent_tid_v = parent_tid();

    bwprintf(COM2, "Child (tid=%d, parent_tid=%d)" EOL, tid_v, parent_tid_v);

    pass();

    bwprintf(COM2, "Child (tid=%d, parent_tid=%d)" EOL, tid_v, parent_tid_v);
}

void init_task(void) {
    int i;
    // create two tasks at a lower priority
    for (i = 0; i < 2; i++) {
        int tid = create(16, &child);
        (void) tid;
        bwprintf(COM2, "Created: %d" EOL, tid);
    }

    // create two tasks at a higher priority
    for (i = 0; i < 2; i++) {
        int tid = create(14, &child);
        (void) tid;
        bwprintf(COM2, "Created: %d" EOL, tid);
    }

    bwprintf(COM2, "First: exiting" EOL);
}

int main(int argc, char *argv[]) {
    boot(init_task, 15);
}
