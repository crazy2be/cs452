#include "signal.h"
#include <kernel.h>

void signal_send(int tid) {
	send(tid, NULL, 0, NULL, 0);
}

int signal_recv(void) {
	int tid = -1;
	receive(&tid, NULL, 0);
	reply(tid, NULL, 0);
	return tid;
}
