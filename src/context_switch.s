.text
.globl enter_kernel
.globl exit_kernel
.globl init_task
.globl pass

@ exposed externally
pass:
    swi 0
    bx r14

init_task:
    @ use of r3 as temp storage
    mov r3, #0

    @push onto stack in reverse order
    str r1, [r0, #-4]! @ r15 - program counter, set to address of code to start at
    @ in the actual context switch, these two are saved after returning to supervisor mode
    str r3, [r0, #-4]! @ r14 - TODO: we should initialize lr so that we jump to exit() at the end
    @ don't save stack pointer
    @ save frame pointer with same value as stack pointer will be at the end of this function
    sub r2, r0, #56 @ reduce by 56 bytes to match stack value at the end (14 more registers * 4 bytes each)
    str r2, [r0, #-4]! @ r12 - frame pointer, set to the same as stack pointer

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

    @psr is the first thing loaded from the stack, so it's the last thing popped on
    str r3, [r0, #-4]! @ psr TODO actually have a sane value for this

    bx r14 @ return

@ method to context switch out of the kernel
@ first argument is the stack pointer of the task we're switching to
exit_kernel:
    @ first, save the kernel's state
    @ save r0-r12, then save load register as the program counter
    @ TODO: we don't need to save r0-r3 here
    @ TODO: surely there is a more efficient way to do this?
    push {r14} @ save LR as PC for when switching context back TODO: is this even used?
    @push {r14} @ save LR as LR (this value is probably thrown away)
    @ don't save the stack pointer
    push {r12}
    push {r11}
    push {r10}
    push {r9}
    push {r8}
    push {r7}
    push {r6}
    push {r5}
    push {r4}

    @ save the psr
    mrs r4, cpsr
    @push {r4} TODO enable this

    @ TODO: should just load the PSR from the stack, not do masking
    @ mask in the right bits to go to user mode
    and r4, r4, #0xffffffe0
    orr r4, r4, #0x10
    msr cpsr, r4

    @ move user stack pointer into position
    @ this has to be done after we enter user mode
    mov sp, r0

    @ load psr; currently we do nothing with this
    pop {r4}

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
    stmfd r1!, {r0, r2}

    @ store the program counter on the stack, in spot reserved for it
    str lr, [r1, #64]

    @ restore the kernel stack
    @ this must correspond to what is being saved in exit_kernel
    pop {r4-r12,lr}

    @ return the user stack pointer
    @ strangely, because we are returning to the point saved by the lr,
    @ this actually returns from the exit_kernel function
    mov r0, r1
    bx lr
