#include "timer.h"

#ifdef QEMU
#define TIMER_BASE           0x101E2000
#define DEBUG_TIMER_BASE     0x101E2020

#define TIMER_ENABLE_MASK    0x80
#define TIMER_PERIODIC_MASK  0x40
#define TIMER_INT_MASK       0x20
#define TIMER_32BIT_MASK     0x02
#define TIMER_16PRE_MASK     0x04
#define TIMER_256PRE_MASK    0x08
#define TIMER_ONE_SHOT_MASK  0x01

#define TIMER_INIT_MASK (TIMER_ENABLE_MASK | TIMER_32BIT_MASK | \
    TIMER_256PRE_MASK | TIMER_PERIODIC_MASK | TIMER_INT_MASK)
#else
#include "ts7200.h"
#define TIMER_BASE           TIMER3_BASE
#define TIMER_INIT_MASK (ENABLE_MASK | MODE_MASK | CLKSEL_MASK)
#define DEBUG_TIMER_BASE     0x80810060
#define DEBUG_TIMER_LO_OFFSET 0x0
#define DEBUG_TIMER_HI_OFFSET 0x4
#define DEBUG_TIMER_ENABLE_MASK 0x100
#endif

#define TIMER_LOAD_OFFSET    0x0
#define TIMER_VALUE_OFFSET   0x4
#define TIMER_CONTROL_OFFSET 0x8
#define TIMER_INTCLR_OFFSET  0xC

#define TIMER_TICK_LEN (TIME_SECOND / 100)

// these definitions are dependent on the hardware, but the two SOCs we support
// are similar enough that we just have to provide different constants
void timer_init(void) {
	// enable the interrupt-generating timer (tick timer)
	volatile unsigned int* value = (unsigned*)(TIMER_BASE + TIMER_LOAD_OFFSET);
	*value = TIMER_TICK_LEN;
	volatile unsigned int* reg = (unsigned*)(TIMER_BASE + TIMER_CONTROL_OFFSET);
	*reg = TIMER_INIT_MASK;

	// enable the debug timer
#ifdef QEMU
	// use a normal timer on the versatilepb
	value = (unsigned*)(DEBUG_TIMER_BASE + TIMER_LOAD_OFFSET);
	*value = 0xffffffff;
	reg = (unsigned*)(DEBUG_TIMER_BASE + TIMER_CONTROL_OFFSET);
	*reg = TIMER_ENABLE_MASK | TIMER_32BIT_MASK | TIMER_ONE_SHOT_MASK;
#else
	// use the 40-bit debug timer on the TS7200
	reg = (unsigned*)(DEBUG_TIMER_BASE + DEBUG_TIMER_HI_OFFSET);
	*reg = DEBUG_TIMER_ENABLE_MASK;
#endif
}

/* unsigned timer_time(void) { */
/* 	return *(volatile unsigned*)(TIMER_BASE + TIMER_VALUE_OFFSET); */
/* } */

void tick_timer_clear_interrupt(void) {
	volatile unsigned *clr = (unsigned*)(TIMER_BASE + TIMER_INTCLR_OFFSET);
	*clr = 0; // data is arbitrary; any write clears the interrupt
}

unsigned debug_timer_useconds(void) {
	volatile unsigned int* value;
#ifdef QEMU
	value = (unsigned*)(DEBUG_TIMER_BASE + TIMER_VALUE_OFFSET);
	// the counter counts down from 0xffffffff, so we have to subtract its value
	return 0xffffffff - *value;
#else
	value = (unsigned*)(DEBUG_TIMER_BASE + DEBUG_TIMER_LO_OFFSET);
	// the debug timer has a 983 KHz clock, so we need to upscale slightly
	unsigned ticks = *value;
	// do a little bit of strangeness in order to not overflow
	return ticks + ticks / 983 * 17;
#endif
}
