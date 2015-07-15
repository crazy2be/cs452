#pragma once

// wrappers of send and receive

// signal the task with that tid
void signal_send(int tid);

// wait for a signal, and return the tid of the signalling task
int signal_recv(void);
