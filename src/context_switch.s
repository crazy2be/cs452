.text
.globl enter_kernel
.globl exit_kernel
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
    stmfd sp!, {r14} @ save LR so we know where to return when context switching back to the kernel
    @ don't save the stack pointer.
    @ save the rest of the registers, excluding r0-r3
    stmfd sp!, {r12}
    stmfd sp!, {r11}
    stmfd sp!, {r10}
    stmfd sp!, {r9}
    stmfd sp!, {r8}
    stmfd sp!, {r7}
    stmfd sp!, {r6}
    stmfd sp!, {r5}
    stmfd sp!, {r4}
    stmfd sp!, {r0}

    @ save the supervisor psr
    mrs r4, cpsr
    stmfd sp!, {r4}

    @ restore user psr
    ldr r4, [r1], #4
    msr cpsr, r4

    @ move user stack pointer into position
    @ this has to be done after we enter sys mode
    mov sp, r1

    @ restore all the variables
    ldmfd sp!, {r0-r12,r14-r15}

enter_kernel:
    @ push a register onto the kernel stack to make room
    stmfd sp!, {r0}

    @ then load cpsr into r0, and mask in the right bits to go to system mode
    mrs r0, cpsr
    orr r0, r0, #0x1f
    msr cpsr, r0

    @ then, save program state on the user stack
    @ this format must match exactly with how the user state is being saved
    @ in init_task and how it is loaded in exit_kernel
    @ leave a spot for the program counter
    sub sp, sp, #4

    stmfd sp!, {r14}
    @ as usual, don't save the stack pointer
    stmfd sp!, {r12}
    stmfd sp!, {r11}
    stmfd sp!, {r10}
    stmfd sp!, {r9}
    stmfd sp!, {r8}
    stmfd sp!, {r7}
    stmfd sp!, {r6}
    stmfd sp!, {r5}
    stmfd sp!, {r4}
    stmfd sp!, {r3}
    stmfd sp!, {r2}
    stmfd sp!, {r1}

    @ r0 is on the supervisor stack, so switch back to that

    @ first, save the user stack pointer
    mov r1, sp

    and r0, r0, #0xffffffe0
    orr r0, r0, #0x13
    msr cpsr, r0

    @ we are now back in supervisor mode, with the kernel stack

    ldmfd sp!, {r0}
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
    ldmfd sp!, {r0}
    msr cpsr, r0

    @ get the syscall number
    ldr r0, [lr, #-4]
    @ the least-significant byte of the swi instruction is the syscall number
    and r0, r0, #0xff

    @ now, restore the rest of the registers
    @ note: what was r0 in the original context is now r3
    ldmfd sp!, {r3-r12,lr}


    @ return the user stack pointer
    @ strangely, because we are returning to the point saved by the lr,
    @ this actually returns from the exit_kernel function

    @ r3 is the value of r0 originally passed into exit_kernel
    @ this is apparently how c returns a struct value
	stmia r3, {r0, r1}
    bx lr
