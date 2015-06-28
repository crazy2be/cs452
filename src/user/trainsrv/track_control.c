#include "track_control.h"
#include <assert.h>
#include <io_server.h>
#include "trainsrv_internal.h"

void tc_set_speed(int train, int speed) {
	ASSERT(1 <= train && train <= 80);
	ASSERT(0 <= speed && speed <= 14);
#ifdef QEMU
	ASSERT(fputs("Changing train speed" EOL, COM1) == 0);
#else
	char train_command[2] = {speed, train};
	ASSERT(fput_buf(train_command, sizeof(train_command), COM1) == 0);
#endif
}

void tc_toggle_reverse(int train) {
	ASSERT(1 <= train && train <= 80);
#ifdef QEMU
	ASSERT(fputs("Reversing train" EOL, COM1) == 0);
#else
	char train_command[2] = {15, train};
	ASSERT(fput_buf(train_command, sizeof(train_command), COM1) == 0);
#endif
}

void tc_stop(int train) {
	ASSERT(1 <= train && train <= 80);
#ifdef QEMU
	ASSERT(fputs("Stopping train" EOL, COM1) == 0);
#else
	char train_command[2] = {0, train};
	ASSERT(fput_buf(train_command, sizeof(train_command), COM1) == 0);
#endif
}

void tc_switch_switch(int sw, enum sw_direction d) {
	ASSERT((1 <= sw && sw <= 18) || (145 <= sw && sw <= 148) || (150 <= sw && sw <= 156));
#ifdef QEMU
	ASSERT(fputs("Changing switch position" EOL, COM1) == 0);
#else
	char cmd = (d == STRAIGHT) ? 0x21 : 0x22;
	char sw_command[2] = {cmd, sw};
	ASSERT(fput_buf(sw_command, sizeof(sw_command), COM1) == 0);
#endif
}

void tc_deactivate_switch(void) {
#ifdef QEMU
	ASSERT(fputs("Resetting solenoid" EOL, COM1) == 0);
#else
	ASSERT(fputc(0x20, COM1) == 0);
#endif
}
