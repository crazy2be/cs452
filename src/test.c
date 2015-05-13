#include "bwio.h"
#include "task_descriptor.h"

static struct task_context old_context, new_context;

void context_switch() {
    register unsigned sp asm("r13");
    unsigned sp2 = sp;
    /* __asm__("stmfd r13,{r0-r12,r14}"); */
    bwprintf(COM2, "Hello world from cs 1 %x\n\r", sp2);
    __asm__("stmfd r13,{r0-r9}");
    sp2 = sp;
    bwprintf(COM2, "Hello world from cs 2 %x\n\r", sp2);

    /* old_context.stack_pointer = sp; */

    /* sp = new_context.stack_pointer; */
    /* __asm__("LDMFD r13,{r0-r12,r14}"); */

    /* sp = old_context.stack_pointer; */
    /* __asm__("ldmfd r13,{r0-r12,r14}"); */
    __asm__("ldmfd r13,{r0-r9}");
    bwprintf(COM2, "Hello world from cs 3\n\r");
    /* __asm__("BX r14"); */
}

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
