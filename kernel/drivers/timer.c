#include "timer.h"

#ifdef QEMU
#define TIMER_BASE           0x101E2000

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
#endif

#define TIMER_LOAD_OFFSET    0x0
#define TIMER_VALUE_OFFSET   0x4
#define TIMER_CONTROL_OFFSET 0x8
#define TIMER_INTCLR_OFFSET  0xC
#define TIMER_TICK_LEN 0xffffffff

// these definitions are dependent on the hardware, but the two SOCs we support
// are similar enough that we just have to provide different constants
void timer_init(void) {
	volatile unsigned int* value = (unsigned*)(TIMER_BASE + TIMER_LOAD_OFFSET);
    *value = TIMER_TICK_LEN;
	volatile unsigned int* reg = (unsigned*)(TIMER_BASE + TIMER_CONTROL_OFFSET);
    *reg = TIMER_INIT_MASK;
}

unsigned timer_time(void) {
	return *(volatile unsigned*)(TIMER_BASE + TIMER_VALUE_OFFSET);
}

void timer_clear_interrupt(void) {
    volatile unsigned *clr = (unsigned*)(TIMER_BASE + TIMER_INTCLR_OFFSET);
    *clr = 0; // data is arbitrary; any write clears the interrupt
}

// generic, architecture independant definitions

int time_hours(long long time) {
	return time / (TIME_SECOND * 60 * 60) % 24;
}
int time_minutes(long long time) {
	return time / (TIME_SECOND * 60) % 60;
}
int time_seconds(long long time) {
	return time / TIME_SECOND % 60;
}
int time_useconds(long long time) {
	return (time*1000000) / TIME_SECOND % 1000000;
}
int time_fraction(long long time) {
	return time % TIME_SECOND;
}
