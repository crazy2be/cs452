#include <kernel.h>
#include "util.h"
#include "task_descriptor.h"
#include "io.h"
#include "drivers/timer.h"
#include "context_switch.h"

#define MAX_TID 255
#define USER_STACK_SIZE 0x1000 // 4K stack

void test3(void) {
    io_puts(COM2, "Inside test function 3\n\r");
    io_flush(COM2);
}

void test2(void) {
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

struct task_collection {
    // for now, a TID is just an index into this array
    // eventually, this will no longer scale
    struct task_descriptor task_buf[MAX_TID + 1];
    // next tid to allocate
    int next_tid;
    void *memory_alloc;
};

static struct task_collection tasks;

struct task_descriptor *create_task(void *entrypoint, int priority, int parent_tid) {
    void *sp = tasks.memory_alloc;
    tasks.memory_alloc += USER_STACK_SIZE;
    struct task_descriptor *task = &tasks.task_buf[tasks.next_tid];

    task->tid = tasks.next_tid++;
    task->parent_tid = parent_tid;
    task->priority = priority;
    /* task->state = READY; */
    /* task->memory_segment = sp; */

    struct user_context *uc = ((struct user_context*) sp) - 1;
    uc->pc = (unsigned) entrypoint;
    uc->lr = (unsigned) &exitk;
    // leave most everything else uninitialized
    uc->r12 = (unsigned) sp;
    unsigned cpsr;
    __asm__ ("mrs %0, cpsr" : "=r" (cpsr));
    uc->cpsr = cpsr & 0xfffffff0;

    task->context.stack_pointer = (void*) uc;

    return task;
}

int main(int argc, char *argv[]) {
	uart_configure(COM1, 2400, OFF);
	uart_configure(COM2, 115200, OFF);

	uart_clrerr(COM1);
	uart_clrerr(COM2);

	while (!uart_canwrite(COM2)) {} uart_write(COM2, 'B');
	while (!uart_canwrite(COM2)) {} uart_write(COM2, 'o');
	while (!uart_canwrite(COM2)) {} uart_write(COM2, 'o');
	while (!uart_canwrite(COM2)) {} uart_write(COM2, 't');

	// Zero BSS, because we should.
	extern char __bss_start__, __bss_end__;
	memset(&__bss_start__, 0, &__bss_end__ - &__bss_start__);
	while (!uart_canwrite(COM2)) {} uart_write(COM2, '.');

	timer_init();
	while (!uart_canwrite(COM2)) {} uart_write(COM2, '.');
	io_init();
	while (!uart_canwrite(COM2)) {} uart_write(COM2, '.');

	io_puts(COM2, "IO...\n\r");
	io_flush(COM2);

	printf("e1:%x ", uart_err(COM1));
	printf("e2:%x ", uart_err(COM2));
	io_flush(COM2);

	// Copy exception vector from where it's linked/loaded to the start of
	// memory, where ARM expects to find it. Assumes all instructions in the
	// exception vector are branch instructions.
	// http://www.ryanday.net/2010/09/08/arm-programming-part-1/
	extern volatile int exception_vector_table_src_begin;
	volatile int* exception_vector_table_src = &exception_vector_table_src_begin;
	volatile int* exception_vector_table_dst = (volatile int*)0x0;
	// Note this is automatically divided by 4 because pointers.
	unsigned exception_vector_branch_adjustment =
		exception_vector_table_src - exception_vector_table_dst;
	for (int i = 0; i < 8; i++) {
		exception_vector_table_dst[i] = exception_vector_table_src[i]
			+ exception_vector_branch_adjustment;
	}

    struct task_descriptor * current_task;
    struct priority_task_queue queue;
    priority_task_queue_init(&queue);

    tasks.next_tid = 0;
    tasks.memory_alloc = (void*) 0x200000;

    // set up the first task

    current_task = create_task(&test2, 1, 0);
    (void) current_task;

	io_puts(COM2, "Starting task scheduling\n\r");
	io_flush(COM2);

    do {
        struct syscall_context sc;
        struct task_descriptor *new_task;

        sc = exit_kernel(current_task->context.stack_pointer);
        current_task->context.stack_pointer = sc.stack_pointer;
        struct user_context *uc = (struct user_context*) sc.stack_pointer;

        switch (sc.syscall_num) {
        case SYSCALL_PASS:
            priority_task_queue_push(&queue, current_task);
            break;
        case SYSCALL_EXIT:
            break;
        case SYSCALL_TID:
            uc->r0 = current_task->tid;
            priority_task_queue_push(&queue, current_task);
            break;
        case SYSCALL_PARENT_TID:
            uc->r0 = current_task->parent_tid;
            priority_task_queue_push(&queue, current_task);
            break;
        case SYSCALL_CREATE:
            new_task = create_task((void*) uc->r1, (int) uc->r0, current_task->tid);
            priority_task_queue_push(&queue, new_task);
            // TODO: do error handling
            uc->r0 = new_task->tid;
            priority_task_queue_push(&queue, current_task);
            break;
        default:
            assert(0, "UNKNOWN SYSCALL NUMBER");
            break;
        }

        current_task = priority_task_queue_pop(&queue);
    } while (current_task);

	io_puts(COM2, "No more tasks; done task scheduling\n\r");
	io_flush(COM2);

    return 0;
}

