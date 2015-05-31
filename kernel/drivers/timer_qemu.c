#ifdef QEMU
#include "timer.h"

#include "ts7200.h"
// https://singpolyma.net/2012/02/writing-a-simple-os-kernel-hardware-interrupts/
#define TIMER0 ((volatile unsigned int*)0x101E2000)
#define TIMER_LOAD 0x0    /* 0x00 bytes */
#define TIMER_VALUE 0x1   /* 0x04 bytes */
#define TIMER_CONTROL 0x2 /* 0x08 bytes */
#define TIMER_INTCLR 0x3  /* 0x0C bytes */
#define TIMER_MIS 0x5     /* 0x14 bytes */

// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0271d/index.html
#define TIMER_ENABLE 0x80
#define TIMER_PERIODIC 0x40
#define TIMER_INT 0x20
#define TIMER_32BIT 0x02
#define TIMER_16PRE 0x04
#define TIMER_256PRE 0x08
#define TIMER_ONE_SHOT 0x01

void timer_init(void) {
	volatile unsigned int* value = TIMER0 + TIMER_LOAD;
    *value = 801;
	volatile unsigned int* reg = TIMER0 + TIMER_CONTROL;
	int data = *reg;
	data |= TIMER_ENABLE; // Enable timer
	data |= TIMER_32BIT; // Use 32 bits
	data |= TIMER_256PRE; // Scale timer down 256 times (1Mhz default).
    data |= TIMER_PERIODIC;
    data &= ~TIMER_ONE_SHOT;
    /* data &= ~TIMER_INT; */
	*reg = data;
}
long long timer_time(void) {
	return -*(TIMER0 + TIMER_VALUE);
//	return -(long long)*(int*)(TIMER3_BASE + VAL_OFFSET) + ((long long)s_cwraps << 32);
}

void timer_clear_interrupt(void) {
    volatile unsigned *clr = TIMER0 + TIMER_INTCLR;
    *clr = 0; // data is arbitrary; any write clears the interrupt
}
#endif
