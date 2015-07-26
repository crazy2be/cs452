.global exception_vector_table_src_begin

.text
exception_vector_table_src_begin:
@ http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0211h/Babfeega.html
b Reset_Handler         @ Reset
b undefined_instruction @ Undefined Instruction
b enter_kernel          @ Software Interrupt
b prefetch_abort        @ Prefetch abort (invalid instruction?)
b data_abort            @ Data abort (invalid memory access?)
b .                     @ Reserved
b enter_kernel_irq      @ IRQ
b .                     @ FRQ

put: @ Local utility "function". Put string to put in r1
	stmfd sp!, {lr}
	mov r0, #2 @ COM2_DEBUG
	bl fprintf
	ldmfd sp!, {pc}

@ 0x201802c
.macro exception_occured_m msg
	stmfd sp!, {r0-r12, r14, r15} @ All but sp
	ldr r1, =\msg
	bl put
	ldr r1, =bsod_string
    bl put
    mov r1, sp
    bl print_stacked_registers
	b .
.endm

.align
.globl undefined_instruction
undefined_instruction: exception_occured_m undefined_instruction_msg
.align
.globl prefetch_abort
prefetch_abort: exception_occured_m prefetch_abort_msg
.align
.globl data_abort
data_abort: exception_occured_m data_abort_msg

.data
undefined_instruction_msg: .ascii "Undefined instruction!\0"
prefetch_abort_msg: .ascii "Prefetch abort!\0"
data_abort_msg: .ascii "Data abort!\0"

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
.ascii "    An exception ?? has occured at %08x in ????. It may be possible to continue normally.\r\n\x1B[2K"
.ascii "\r\n\x1B[2K"
.ascii "        * Press any key to attempt to continue\r\n\x1B[2K"
.ascii "        * Press CTRL+ALT+DELETE to reset your trains. You will lose any unsaved trains.\r\n\x1B[2K"
.ascii "\r\n\x1B[2K"
.ascii "    Press any key to continue...\r\n\x1B[2K"
.ascii "\x1B[0m" @ reset colors
.ascii "\x1B[u"
.ascii "\0"
