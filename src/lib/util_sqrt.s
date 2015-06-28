.text
.globl sqrti


@ fast integer integer square root routine
@ sourced from http://www.finesse.demon.co.uk/steven/sqrt.html, under
@ 3 cycle/bit routine

@ r0 is N, the number we want to square root
@ r1 is root, the intermediate result
@ r2 is offset, which is a constant

@ offset only has it's own register because we can't abuse flexible
@ second operands to calculate it on the fly

.macro sqrt_iteration i
	cmp r0, r1, ror #\i
	subhs r0, r0, r1, ror #\i
	adc r1, r2, r1, lsl #1
.endm

sqrti:
	mov r2, #0xc0000000
	mov r1, #0x40000000
	sqrt_iteration 0
	sqrt_iteration 2
	sqrt_iteration 4
	sqrt_iteration 6
	sqrt_iteration 8
	sqrt_iteration 10
	sqrt_iteration 12
	sqrt_iteration 14
	sqrt_iteration 16
	sqrt_iteration 18
	sqrt_iteration 20
	sqrt_iteration 22
	sqrt_iteration 24
	sqrt_iteration 26
	sqrt_iteration 28
	sqrt_iteration 30
	bic r0, r1, #0xc0000000
	bx lr
