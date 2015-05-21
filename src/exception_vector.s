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

.align 8
press_key: @ Waits for the user to press a key
	stmfd sp!, {lr}
	loop1:
		mov r0, #1 @ COM2
		bl uart_canread
		cmp r0, #0
		beq loop1 @ No character
	mov r0, #1 @ COM2
	bl uart_read
	ldmfd sp!, {lr}

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
	ldr r1, =bsod_string
	bl puts
	bl press_key
	ldmfd sp!, {r4-r11, r15} @ TODO: Should we exit here, instead?

bsod_string:
@ TODO: This error message should be more specific, I think ARM gives us a
@ mechanism for finding the exact address(es) that were involved in the error.
.ascii "\x1B[s"
.ascii "\x1B[1;1H"
.ascii "\x1B[44;37m"
.ascii "\x1B[2K"
.ascii "                    "
.ascii "\x1B[34;47;1m"
.ascii " Trains "
.ascii "\x1B[44;37;22m"
.ascii "\r\n\x1B[2K"
.ascii "\r\n\x1B[2K"
.ascii "    An exception ?? has occured at ????:???????? in ????. It may be possible to continue normally.\r\n\x1B[2K"
.ascii "\r\n\x1B[2K"
.ascii "        * Press any key to attempt to continue\r\n\x1B[2K"
.ascii "        * Press CTRL+ALT+DELETE to reset your trains. You will lose any unsaved trains.\r\n\x1B[2K"
.ascii "\r\n\x1B[2K"
.ascii "    Press any key to continue...\r\n\x1B[2K"
.ascii "\x1B[0m" @ reset colors
.ascii "\x1B[u"
.ascii "\0"
