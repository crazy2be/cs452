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
void timer_service() {
//	int *reg = (int*)(TIMER3_BASE + VAL_OFFSET); // Counting DOWN
//	int data = *reg;
	// Wrap around
//	if (data > s_timer_prev) s_cwraps++;
}
long long timer_time() {
	return -*(int*)(TIMER3_BASE + VAL_OFFSET);
//	return -(long long)*(int*)(TIMER3_BASE + VAL_OFFSET) + ((long long)s_cwraps << 32);
}
int time_hours(long long time) {
	return time / (TIME_SECOND * 60 * 60) % 24;
}
int time_minutes(long long time) {
	return time / (TIME_SECOND * 60) % 60;
}
int time_seconds(long long time) {
	return time / TIME_SECOND % 60;
}
int time_fraction(long long time) {
	return time % TIME_SECOND;
}
#endif
