#pragma once

// two timers are provided by this file
//  1. A timer which generates an interrupt every 10ms (1 tick).
//     We don't otherwise interact with this timer directly.
//  2. A debug timer which continuously counts up from the time
//     we start running the program. This is used by the kernel to
//     do performance measurement.
//     The timer will overflow in ~1h, but that's acceptable for our purposes.

#if QEMU
#define TIME_SECOND (1000000/256) // 1Mhz / 256
#else
//#define TIME_SECOND 2000 // 2kHz
#define TIME_SECOND 508000 // 508kHz
#endif

void timer_init(void);

/* unsigned timer_time(void); */
void tick_timer_clear_interrupt(void);

unsigned debug_timer_useconds(void);
