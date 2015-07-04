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

// sends a message back to the send, instead of replying
// does not block
// example use: delay_async(50, &msg, sizeof(msg), offsetof(msg.ticks))
void delay_async(int ticks, void *msg, unsigned msglen, unsigned msg_tick_offset);
