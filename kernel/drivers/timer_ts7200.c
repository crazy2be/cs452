#ifndef QEMU
#include "timer.h"

#include "ts7200.h"

void timer_init() {
	int* reg = (int*)(TIMER3_BASE + CRTL_OFFSET); // Timer 3 is 32 bits, use that one.
	int data = *reg;
	data = data | ENABLE_MASK; // Enable timer
	data = data & ~MODE_MASK; // Free running timer mode
	//data = data & ~CLKSEL_MASK; // 2khz Clock
	data = data | CLKSEL_MASK; // 508khz Clock
	*reg = data;
}

long long timer_time() {
	return -*(int*)(TIMER3_BASE + VAL_OFFSET);
//	return -(long long)*(int*)(TIMER3_BASE + VAL_OFFSET) + ((long long)s_cwraps << 32);
}
#endif
