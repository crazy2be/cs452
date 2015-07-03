#include "track_control.h"
#include <assert.h>
#include <io.h>
#include "trainsrv_internal.h"
#include "../sys.h" // delay()

struct switch_state tc_init_switches(void) {
	struct switch_state switches = {};
	for (int i = 1; i <= 18; i++) {
		tc_switch_switch(i, CURVED);
		switch_set(&switches, i, CURVED);
		// Avoid flooding rbuf. We probably shouldn't need this, since rbuf
		// should have plenty of space, but it seems to work...
		delay(1);
	}
	tc_switch_switch(153, CURVED);
	switch_set(&switches, 153, CURVED);
	tc_switch_switch(156, CURVED);
	switch_set(&switches, 156, CURVED);
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
