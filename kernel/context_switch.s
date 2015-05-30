.text
.globl enter_kernel
.globl enter_kernel_irq
.globl exit_kernel
.globl pass

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

@ Some general notes on restoring registers:

@ The restore order for user context is lr, pc, psr, r0-r12
@ This is therefore the order that the registers are stored on the stack,
@ with lr at the lowest addr, and r12 at the highest addr.
@ Note that the save order is the reverse of the restore order.

@ Further, remember that stmfd sp!, {r0, r1} <=> stmfd sp!, {r1}; stmfd sp!b{r0}
@ whereas ldmfd sp!, {r0, r1} <=> ldmfd sp!, {r0}; ldmfd sp!, {r1}

@ The stack pointer value for the kernel is persisted in its register,
@ so we don't need to worry about restoring that

@ state save of user task should be 16 registers

@ state save of kernel should be 11 registers (r0, r4-r12, r14)
@ we can save less since we don't care about r1-r3, and r13
@ is preserved for us
@ we also don't care about cpsr, since the compiler doesn't assume
@ this is preserved by the call to exit_kernel
@ r0 is a pointer to the struct returned by exit_kernel

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

@ method to context switch out of the kernel
@ first argument is the stack pointer of the task we're switching to
exit_kernel:
    @@@@@ SUPERVISOR MODE @@@@@

    @ expected arguments:
    @ r0 is where C expects us to write the struct return value
    @ r1 is the user task's stack pointer

    @ first, save the kernel's state.
    @ this needs to match exactly with how it's loaded back in enter_kernel.

    @ save LR since the LR gets overwritten when a user task switches into the kernel
    @ save r4-r12, since these registers are expected to be untouched by this call
    @ save r3, since this is the address that C expects us to write the return struct into
    @ save r2 (value of cpsr) to be able to restore the kernel psr (TODO: necessary?)
    stmfd sp!, {r0, r4-r12, r14}

    @ Now, restore the user state
    @ Restore the SP and LR while in system mode
    @ Then return to supervisor mode and restore r0-r12
    @ Then restore PC and PSR atomically

    mrs r3, cpsr
    orr r3, r3, #0x1f
    msr cpsr, r3

    @@@@@ SYSTEM MODE @@@@@

    @ TODO: probably should reorder the way we store registers on the stack for efficiency
    @ let's just get it working first

    @ restore the user stack pointer and link register

    @ we want the value of the stack pointer after all the values have been popped
    @ of, so we add 64 = 16 registers * 4 bytes
    add sp, r1, #64

    ldmfd r1!, {lr}

    @ switch back to supervisor mode
    and r3, r3, #0xfffffff3
    msr cpsr, r3

    @@@@@ SUPERVISOR MODE @@@@@

    @ restore user psr by popping it off the user task stack
    @ then move it to spsr, since `movs pc, lr` expects it to there
    ldmfd r1!, {r4, lr}
    msr spsr, r4

    @ restore all the variables
    ldmfd r1, {r0-r12}

    @ restore pc and cpsr atomically
    @ pc <- lr; cpsr <- spsr
    movs pc, lr

    @@@@@ USER MODE @@@@@

enter_kernel:
    @ We have to do a little bit of dancing back and forth to save all
    @ the registers.
    @ We need to r0-r12,r14,r14_svc, and spsr
    @ We also need to return r13

    @ First, we have to switch to system mode, so we can even access the user
    @ stack pointer.
    @ This requires the use of a register, so we need to save a register to
    @ the kernel stack, then retrieve it later. We use r0 for this purpose.
    @ In system mode, we save r1-r12,r14 to the user stack.
    @ We then switch back to supervisor mode to save spsr and r14_svc (the saved
    @ value of pc) to the user stack.
    @ Finally, sp is returned into the kernel.

    @@@@@ SUPERVISOR MODE @@@@@

    @ push a register onto the kernel stack to make room
    stmfd sp!, {r0}

    @ then load cpsr into r0, and mask in the right bits to go to system mode
    mrs r0, cpsr
    orr r0, r0, #0x1f
    msr cpsr, r0

    @@@@@ SYSTEM MODE @@@@@

    @ then, save program state on the user stack
    @ this format must match exactly with how the user state is being saved
    @ in init_task and how it is loaded in exit_kernel

    @ save all registers except the stack pointer
    stmfd sp!, {r1-r12}

    @ save the user stack pointer and lr
    mov r1, sp
    mov r2, lr

    @ r0 is on the supervisor stack, so switch back to that

    and r0, r0, #0xfffffff3
    msr cpsr, r0

    @@@@@ SUPERVISOR MODE @@@@@

    @ store user's r0, pc, spsr, and lr on the stack
    @ note that we've shuffled these around into weird registers so that
    @ stmfd saves them in the correct order
    @ they are r5, r4, r3, and r2, respectively

    @ put user's initial r0 into r2
    ldmfd sp!, {r5}

    @ load the spsr (user's original cpsr)
    mrs r3, spsr

    @ move lr into r4 purely to have the correct save order
    mov r4, lr

    stmfd r1!, {r2-r5}

    @ get the syscall number
    ldr r0, [lr, #-4]
    @ the least-significant byte of the swi instruction is the syscall number
    and r0, r0, #0xff

    @ restore the kernel registers
    @ this must correspond to what is being saved in exit_kernel
    @ note: what was r0 in the original context is now r3
    ldmfd sp!, {r3-r12,lr}

    @ return the user stack pointer
    @ strangely, because we are returning to the point saved by the lr,
    @ this actually returns from the exit_kernel function

    @ r3 is the value of r0 originally passed into exit_kernel
    @ this is apparently how c returns a struct value
	stmia r3, {r0, r1}
    bx lr

enter_kernel_irq:
    @ Abuse sp_irq to store cpsr temp
    mrs sp, cpsr
    orr sp, sp, #0x1f
    msr cpsr, sp

    @@@@@ SYSTEM MODE @@@@@

    stmfd sp!, {r0-r12}
    @ save lr_usr in what will be the bottom position in the stack
    str lr, [sp, #-12]

    @ save user stack pointer before returning to IRQ mode
    mov r1, sp

    mrs r0, cpsr
    and r0, r0, #0xfffffff2
    msr cpsr, r0

    @@@@@ IRQ MODE @@@@@

    @ store user pc & cpsr
    @ the link register points to the instruction *after* the one we should
    @ return to
    sub lr, lr, #4
    mrs r2, spsr
    stmfd r1!, {r2, lr}

    @ decrement stack pointer to account for having already stored lr_usr above
    sub r1, r1, #4

    orr r0, r0, #0x13
    msr cpsr, r0

    @@@@@ SVC MODE @@@@@

    @ restore the kernel registers
    @ this must correspond to what is being saved in exit_kernel
    @ note: what was r0 in the original context is now r3
    ldmfd sp!, {r3-r12,lr}

    @ dummy value to return as syscall code
    mov r0, #37

    @ return the user stack pointer
    @ strangely, because we are returning to the point saved by the lr,
    @ this actually returns from the exit_kernel function

    @ r3 is the value of r0 originally passed into exit_kernel
    @ this is apparently how c returns a struct value
	stmia r3, {r0, r1}
    bx lr
