#include "track_control.h"
#include <assert.h>
#include "io_server.h"

void set_train_speed(int train, int speed) {
	ASSERT(1 <= train && train <= 80);
	ASSERT(1 <= speed && speed <= 15);
	char train_command[3] = {speed - 1, train, 0};
	ASSERT(iosrv_puts(COM1, train_command) == 0);
}

void set_switch_state(int sw, enum sw_direction d) {
	ASSERT((1 <= sw && sw <= 18) || (153 <= sw && sw <= 156));
	char cmd = (d == STRAIGHT) ? 0x21 : 0x22;
	char sw_command[3] = {cmd, sw, 0};
	ASSERT(iosrv_puts(COM1, sw_command) == 0);
}

void disable_switch_solenoid(void) {
	ASSERT(iosrv_putc(COM1, 0x20) == 0);
}
