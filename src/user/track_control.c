#include "track_control.h"
#include <assert.h>
#include "io_server.h"

void send_sensor_poll(void) {
	fputc(COM1, 0x85);
}
