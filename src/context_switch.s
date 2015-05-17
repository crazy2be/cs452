.text
.globl enter_kernel
.globl exit_kernel
.globl init_task_stack
.globl pass

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

@ Some general notes on restoring registers:
@ Generally, the restore order is sp, psr, r0-r12, r14-r15.
@ This is therefore the order that the registers are stored on the stack,
@ with psr at the lowest addr, and pc at the highest addr.
@ Note that the save order is the reverse of the restore order.

@ we need to restore the sp as the very first thing, in order to
@ be able to find anything else

@ the stack pointer value for the kernel is persisted in its register,
@ so we don't need to worry about restoring that

@ generally, we want to restore the pc last, and the psr first

@ state save of user task should be 16 registers

@ state save of kernel should be 11 registers (cpsr, r4-r12, r14)
@ we can save less since we don't care about r0-r3, and r13
@ is preserved for us

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

@ write starter values onto the stack of a task, when that task is first created
@ this allows us to simplify our context switching code, since we can now handwavely
@ state that every task is already running, and has a valid stack to resume to
init_task_stack:
    @ use of r3 as temp storage
    mov r3, #0
    mov r2, r0

    @push onto stack in reverse order
    str r1, [r0, #-4]! @ r15 - program counter, set to address of code to start at
    @ in the actual context switch, these two are saved after returning to supervisor mode
    str r3, [r0, #-4]! @ r14 - TODO: we should initialize lr so that we jump to exit() at the end
    @ don't save stack pointer
    str r2, [r0, #-4]! @ r12 - frame pointer, set to the same as original stack pointer

    str r3, [r0, #-4]! @ r11
    str r3, [r0, #-4]! @ r10
    str r3, [r0, #-4]! @ r9
    str r3, [r0, #-4]! @ r8
    str r3, [r0, #-4]! @ r7
    str r3, [r0, #-4]! @ r6
    str r3, [r0, #-4]! @ r5
    str r3, [r0, #-4]! @ r4
    str r3, [r0, #-4]! @ r3
    str r3, [r0, #-4]! @ r2
    str r3, [r0, #-4]! @ r1
    str r3, [r0, #-4]! @ r0

    @ psr is the first thing loaded from the stack, so it's the last thing popped on
    @ use current psr value, but mask out the appropriate bits to be in user mode
    @ there are probably other bits we need to flip (Z, C, etc.)
    mrs r3, cpsr
    and r3, r3, #0xfffffff0
    str r3, [r0, #-4]! @ psr

    bx r14 @ return

@ method to context switch out of the kernel
@ first argument is the stack pointer of the task we're switching to
exit_kernel:
    @ expected arguments:
    @ r0 is where C expects us to write the struct return value
    @ r1 is the user task's stack pointer, which we do actually use

    @ first, save the kernel's state.
    @ this needs to match exactly with how it's loaded back in enter_kernel.

    @ TODO: surely there is a more efficient way to do this?

    @ don't save the pc this would just get thrown away.
    push {r14} @ save LR so we know where to return when context switching back to the kernel
    @ don't save the stack pointer.
    @ save the rest of the registers, excluding r0-r3
    push {r12}
    push {r11}
    push {r10}
    push {r9}
    push {r8}
    push {r7}
    push {r6}
    push {r5}
    push {r4}
    push {r0}

    @ save the supervisor psr
    mrs r4, cpsr
    push {r4}

    @ restore user psr
    ldr r4, [r1], #4
    msr cpsr, r4

    @ move user stack pointer into position
    @ this has to be done after we enter sys mode
    mov sp, r1

    @ restore all the variables
    pop {r0-r12,r14-r15}

enter_kernel:
    @ push a register onto the kernel stack to make room
    push {r0}

    @ then load cpsr into r0, and mask in the right bits to go to system mode
    mrs r0, cpsr
    orr r0, r0, #0x1f
    msr cpsr, r0

    @ then, save program state on the user stack
    @ this format must match exactly with how the user state is being saved
    @ in init_task and how it is loaded in exit_kernel
    @ leave a spot for the program counter
    sub sp, sp, #4

    push {r14}
    @ as usual, don't save the stack pointer
    push {r12}
    push {r11}
    push {r10}
    push {r9}
    push {r8}
    push {r7}
    push {r6}
    push {r5}
    push {r4}
    push {r3}
    push {r2}
    push {r1}

    @ r0 is on the supervisor stack, so switch back to that

    @ first, save the user stack pointer
    mov r1, sp

    and r0, r0, #0xffffffe0
    orr r0, r0, #0x13
    msr cpsr, r0

    @ we are now back in supervisor mode, with the kernel stack

    pop {r0}
    @ load the spsr (user's original cpsr)
    mrs r2, spsr
    stmfd r1!, {r0}
    stmfd r1!, {r2}

    @ store the program counter on the stack, in spot reserved for it
    str lr, [r1, #60]

    @ restore the kernel registers
    @ this must correspond to what is being saved in exit_kernel
    @ load the kernel's original psr
    @ TODO: is this even necessary?
    pop {r0}
    msr cpsr, r0

    @ get the syscall number
    ldr r0, [lr, #-4]
    @ the least-significant byte of the swi instruction is the syscall number
    and r0, r0, #0xff

    @ now, restore the rest of the registers
    @ note: what was r0 in the original context is now r3
    pop {r3-r12,lr}


    @ return the user stack pointer
    @ strangely, because we are returning to the point saved by the lr,
    @ this actually returns from the exit_kernel function

    @ r3 is the value of r0 originally passed into exit_kernel
    @ this is apparently how c returns a struct value
	stmia r3, {r0, r1}
    bx lr
