#ifdef QEMU
#include "timer.h"

#include "ts7200.h"
// https://singpolyma.net/2012/02/writing-a-simple-os-kernel-hardware-interrupts/
#define TIMER0 ((volatile unsigned int*)0x101E2000)
#define TIMER_VALUE 0x1 /* 0x04 bytes */
#define TIMER_CONTROL 0x2 /* 0x08 bytes */
#define TIMER_INTCLR 0x3 /* 0x0C bytes */
#define TIMER_MIS 0x5 /* 0x14 bytes */

// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0271d/index.html
#define TIMER_ENABLE 0x80
#define TIMER_PERIODIC 0x40
#define TIMER_32BIT 0x02
#define TIMER_16PRE 0x04
#define TIMER_256PRE 0x08

void timer_init() {
	volatile unsigned int* reg = TIMER0 + TIMER_CONTROL;
	int data = *reg;
	data = data | TIMER_ENABLE; // Enable timer
	data = data | TIMER_32BIT; // Use 32 bits
	data = data | TIMER_256PRE; // Scale timer down 256 times (1Mhz default).
	*reg = data;
}
void timer_service() {
//	int *reg = (int*)(TIMER3_BASE + VAL_OFFSET); // Counting DOWN
//	int data = *reg;
	// Wrap around
//	if (data > s_timer_prev) s_cwraps++;
}
long long timer_time() {
	return -*(TIMER0 + TIMER_VALUE);
//	return -(long long)*(int*)(TIMER3_BASE + VAL_OFFSET) + ((long long)s_cwraps << 32);
}
// TODO: Do these even belong in the driver file?
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
#endif
