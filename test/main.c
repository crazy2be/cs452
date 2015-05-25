#include <kernel.h>
#include <io.h>

// lurky, but we have to include these to test stuff
#include "../kernel/least_significant_set_bit.h"

#define STRINGIFY2(STR) #STR
#define STRINGIFY1(STR) STRINGIFY2(STR)

#define ASSERT(stmt) {\
    if (!(stmt)) { \
        io_puts(COM2, "ASSERTION FAILED (" __FILE__ ":" STRINGIFY1(__LINE__) ") : " STRINGIFY1(stmt) EOL); \
        io_flush(COM2); \
    } }


void child(void) {
    io_printf(COM2, "Child task %d" EOL, tid());
    io_flush(COM2);
}

void nop(void) {
}

void lssb_tests(void) {
    int i;
    ASSERT(0 == least_significant_set_bit(0xffffffff));
    ASSERT(2 == least_significant_set_bit(0xc));
    for (i = 0; i < 32; i++) {
        pass();
        ASSERT(i == least_significant_set_bit(0x1 << i));
    }
}

struct Msg {
	int foo;
	int bar;
};
struct Reply {
	int foo2;
	int bar2;
};
static int receiving_tid = -1;
void sending_task(void) {
	for (int i = 0; i < 10; i++) {
		struct Msg msg = {0xFFffffff, 0x55555555};
		struct Reply rep;
		send(receiving_tid, &msg, sizeof(msg), &rep, sizeof(rep));
		printf("Sent %d %d, got %d %d\n", msg.foo, msg.bar, rep.foo2, rep.bar2);
	}
}
void receiving_task(void) {
	for (int i = 0; i < 10; i++) {
		struct Msg msg;
		int tid;
		receive(&tid, &msg, sizeof(msg));
		struct Reply rep = {0x74546765, 0x45676367};
		printf("Got %d %d, replying with %d %d\n", msg.foo, msg.bar, rep.foo2, rep.bar2);
		reply(tid, &rep, sizeof(reply));
	}
}

void init_task(void) {
    lssb_tests();
    ASSERT(1);
    ASSERT(create(-1, child) == CREATE_INVALID_PRIORITY);
    ASSERT(create(32, child) == CREATE_INVALID_PRIORITY);
	printf("Creating messaging tasks...");
	receiving_tid = create(PRIORITY_MIN, receiving_task);
	create(PRIORITY_MIN, sending_task);
	printf("Created tasks, init task exiting...");
//     int i;
//     for (i = 0; i < 10; i++) {
//         // create tasks in descending priority order
//         create(PRIORITY_MIN - i, &child);
//     }

//     for (; i < 255; i++) {
//         create(PRIORITY_MIN, &nop);
//     }
//
//     ASSERT(create(4, child) == CREATE_INSUFFICIENT_RESOURCES);
}

int main(int argc, char *argv[]) {
    boot(init_task, PRIORITY_MAX);
}
