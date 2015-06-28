#include "signal.h"
#include <kernel.h>

int signal_try_send(int tid) {
	return try_send(tid, NULL, 0, NULL, 0);
}

int signal_recv(void) {
	int tid;
	int err;
	if ((err = try_receive(&tid, NULL, 0)) != 0) {
		return err;
	} else if ((err = try_reply(tid, NULL, 0)) != 0) {
		return err;
	} else {
		return tid;
	}
}
