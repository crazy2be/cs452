.global exception_vector_table_src_begin

.text
exception_vector_table_src_begin:
@ http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0211h/Babfeega.html
b Reset_Handler @ Reset
b .             @ Undefined Instruction
b enter_kernel  @ Software Interrupt
b .             @ Prefetch abort (invalid instruction?)
b .             @ Data abort (invalid memory access?)
b .             @ Reserved
b .             @ IRQ
b .             @ FRQ
