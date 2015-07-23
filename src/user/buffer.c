#include "buffer.h"
#include <kernel.h>

struct courier_params {
	int dest_tid;
	unsigned msglen;
};

static void courier(void) {
	int tid;
	struct courier_params params;
	receive(&tid, &params, sizeof(params));
	reply(tid, NULL, 0);

	unsigned char msg[params.msglen];
	receive(&tid, &msg, params.msglen);
	reply(tid, NULL, 0);

	send(params.dest_tid, &msg, params.msglen, NULL, 0);
}

void send_async(int tid, void *msg, unsigned msglen) {
	// same priority as nameserver
	int courier_tid = create(PRIORITY_BUFFER_COURIER, courier);
	struct courier_params params = { tid, msglen };
	send(courier_tid, &params, sizeof(params), NULL, 0);
	send(courier_tid, msg, msglen, NULL, 0);
}
