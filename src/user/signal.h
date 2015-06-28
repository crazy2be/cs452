#pragma once

// wrappers of try_send and try_receive

// signal the task with that tid
int signal_try_send(int tid);

// wait for a signal, and return the tid of the signalling task
int signal_recv(void);
