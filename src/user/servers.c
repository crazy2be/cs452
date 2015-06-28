#include "servers.h"
#include "nameserver.h"
#include "clockserver.h"
#include <io_server.h>
#include <kernel.h>

void start_servers(void) {
	create(LOWER(PRIORITY_MAX, 4), nameserver);
	ioserver(LOWER(PRIORITY_MAX, 3), COM1);
	ioserver(LOWER(PRIORITY_MAX, 3), COM2);
	create(LOWER(PRIORITY_MAX, 1), clockserver);
}

void stop_servers(void) {
	// We may want to clean up the way in which we shut down things,
	// but for the moment, this is how it is.
	//
	// In order to shut down this kernel, we have special halt() call,
	// which just stops the kernel in its tracks.
	// Before we call this, though, we need to make sure that any output
	// is flushed from the IO server's buffers.
	// None of the other servers need to be explicitly shut down or otherwise
	// dealt with, since they have no cleanup which needs to be done.
	//
	// In the old days, we would would explicitly send a message to the clockserver,
	// telling it to stop running, and on the next tick, the clockserver notifier
	// would realize that the clockserver shutdown, and stop as well.
	// This wouldn't work for the IO server, since the RX notifier would only wake
	// up the next time a byte of input was received from its UART, which is
	// indefinitely long.

	ioserver_stop(COM1);
	ioserver_stop(COM2);
	halt();
}
