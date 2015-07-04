#include "track_control.h"
#include <assert.h>
#include <io.h>
#include "trainsrv_internal.h"
#include "../sys.h" // delay()

static void setup_sw(struct switch_state *switches, int n, enum sw_direction d) {
	tc_switch_switch(n, d);
	switch_set(switches, n, d);
}
struct switch_state tc_init_switches(void) {
	struct switch_state switches = {};
	for (int i = 1; i <= 18; i++) {
		setup_sw(&switches, i, CURVED);
		// Avoid flooding rbuf. We probably shouldn't need this, since rbuf
		// should have plenty of space, but it seems to work...
		delay(1);
	}
	setup_sw(&switches, 152, CURVED);
	setup_sw(&switches, 156, CURVED);

	// "Big loop" configuration
	setup_sw(&switches, 6, STRAIGHT);
	setup_sw(&switches, 7, STRAIGHT);
	setup_sw(&switches, 8, STRAIGHT);
	setup_sw(&switches, 9, STRAIGHT);
	setup_sw(&switches, 14, STRAIGHT);
	setup_sw(&switches, 15, STRAIGHT);

	tc_deactivate_switch();
	return switches;
}

void tc_set_speed(int train, int speed) {
	ASSERT(1 <= train && train <= 80);
	ASSERT(0 <= speed && speed <= 14);
#ifdef QEMU
	fputs("Changing train speed" EOL, COM1);
#else
	char train_command[2] = {speed, train};
	fput_buf(train_command, sizeof(train_command), COM1);
#endif
}

void tc_toggle_reverse(int train) {
	ASSERT(1 <= train && train <= 80);
#ifdef QEMU
	fputs("Reversing train" EOL, COM1);
#else
	char train_command[2] = {15, train};
	fput_buf(train_command, sizeof(train_command), COM1);
#endif
}

void tc_stop(int train) {
	ASSERT(1 <= train && train <= 80);
#ifdef QEMU
	fputs("Stopping train" EOL, COM1);
#else
	char train_command[2] = {0, train};
	fput_buf(train_command, sizeof(train_command), COM1);
#endif
}

void tc_switch_switch(int sw, enum sw_direction d) {
	ASSERT((1 <= sw && sw <= 18) || (145 <= sw && sw <= 148) || (150 <= sw && sw <= 156));
#ifdef QEMU
	fputs("Changing switch position" EOL, COM1);
#else
	char cmd = (d == STRAIGHT) ? 0x21 : 0x22;
	char sw_command[2] = {cmd, sw};
	fput_buf(sw_command, sizeof(sw_command), COM1);
#endif
}

void tc_deactivate_switch(void) {
#ifdef QEMU
	fputs("Resetting solenoid" EOL, COM1);
#else
	fputc(0x20, COM1);
#endif
}
