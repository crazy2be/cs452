.global exception_vector_table_src_begin

.text
exception_vector_table_src_begin:
@ http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0211h/Babfeega.html
b Reset_Handler         @ Reset
b undefined_instruction @ Undefined Instruction
b enter_kernel          @ Software Interrupt
b prefetch_abort        @ Prefetch abort (invalid instruction?)
b data_abort            @ Data abort (invalid memory access?)
b .              @ Reserved
b .              @ IRQ
b .              @ FRQ

.global fucked_handler
.global fprintf
.align 8
press_key: @ Waits for the user to press a key
	stmfd sp!, {lr}
	ldr r1, =press_key_string
	bl puts
	loop1:
		mov r0, #1 @ COM2
		bl uart_canread
		cmp r0, #0
		beq loop1 @ No character
	mov r0, #1 @ COM2
	bl uart_read
	ldmfd sp!, {lr}
press_key_string:
@ TODO: Win 98 style BSOD!!!
.ascii "\x1B[s"
.ascii "\x1B[1;1H"
.ascii "\x1B[44;1m" @ bold red
.ascii "Press any key to continue...\r\n"
.ascii "\x1B[0m" @ reset colors
.ascii "\x1B[u"
.ascii "\0"

.align 8
puts: @ Local utility "function". Put string to put in r1
	stmfd sp!, {lr}
	mov r0, #1 @ COM2
	bl fprintf
	mov r0, #1 @ COM2
	bl io_flush
	ldmfd sp!, {pc}

.align 8
undefined_instruction:
prefetch_abort:
data_abort:
	stmfd sp!, {r4-r11, r14}
	ldr r1, =fucked_up_string
	bl puts
	bl press_key
	ldmfd sp!, {r4-r11, r15} @ TODO: Should we exit here, instead?
fucked_up_string:
@ TODO: This error message should be more specific, I think ARM gives us a
@ mechanism for finding the exact address(es) that were involved in the error.
.ascii "Fuck. Fuck. Fuck. We should put more debug output here if you're hitting this...\r\n\0"
