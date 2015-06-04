#include "signal.h"
#include <kernel.h>

int signal_send(int tid) {
	return send(tid, NULL, 0, NULL, 0);
}

int signal_recv(void) {
	int tid;
	int err;
	if ((err = receive(&tid, NULL, 0)) != 0) {
		return err;
	} else if ((err = reply(tid, NULL, 0)) != 0) {
		return err;
	} else {
		return tid;
	}
}
