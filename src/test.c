#include "bwio.h"
#include "task_descriptor.h"

static struct task_context old_context, new_context;

void context_switch(void);

void test(void) {
    bwprintf(COM2, "Hello world from function\n\r");
}

int main (int argc, char *argv[]) {
    int test2 = 7;
    void (*fp)(void) = &test;
    bwsetfifo(COM2, 1);
    bwsetspeed(COM2, 115200);
    bwprintf(COM2, "Hello world from main\n\r");

    // set up pretend link register in other stack
    new_context.stack_pointer = 0x80000;
    *(int*) new_context.stack_pointer = (unsigned) &test;
    context_switch();

    bwprintf(COM2, "Hello world from main again here\n\r");

    /* fp(); */
    return test2;
}
