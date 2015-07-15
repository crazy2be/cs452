#pragma once

void clockserver(void);

// all time is represented in the number of ticks since the start of the clock
// server
// time() returns immediately with the time
// delay & delay_until delay for a number of ticks, or until a particular time,
// then return the time
int time();
int delay(int ticks);
int delay_until(int ticks);
