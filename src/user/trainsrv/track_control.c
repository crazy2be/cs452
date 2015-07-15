#include "track_control.h"
#include <assert.h>
#include <io.h>
#include "trainsrv_internal.h"
#include "../sys.h" // delay()

void tc_switch_switches_bulk(const struct switch_state switches) {
	for (int i = 1; i <= 18; i++) {
		tc_switch_switch(i, switch_get(&switches, i));
	}
	for (int i = 145; i <= 156; i++) {
		tc_switch_switch(i, switch_get(&switches, i));
	}
	tc_deactivate_switch();
}
static void setup_big_loop(struct switch_state *switches) {
	switch_set(switches, 6, STRAIGHT);
	switch_set(switches, 7, STRAIGHT);
	switch_set(switches, 8, STRAIGHT);
	switch_set(switches, 9, STRAIGHT);
	switch_set(switches, 14, STRAIGHT);
	switch_set(switches, 15, STRAIGHT);
}
static void setup_medium_loop(struct switch_state *switches) {
	switch_set(switches, 10, STRAIGHT);
	switch_set(switches, 13, STRAIGHT);
	switch_set(switches, 16, STRAIGHT);
	switch_set(switches, 17, STRAIGHT);
}
struct switch_state tc_init_switches(void) {
	struct switch_state switches = {};
	for (int i = 1; i <= 18; i++) {
		switch_set(&switches, i, CURVED);
	}
	switch_set(&switches, 153, CURVED);
	switch_set(&switches, 156, CURVED);
	(void) setup_big_loop;
	setup_medium_loop(&switches);

	tc_switch_switches_bulk(switches);
	return switches;
}

void tc_set_speed(int train, int speed) {
	ASSERT(1 <= train && train <= 80);
	ASSERT(0 <= speed && speed <= 14);
#ifdef ASCII_TC
	fputs("Changing train speed" EOL, COM1);
#else
	char train_command[2] = {speed, train};
	fput_buf(train_command, sizeof(train_command), COM1);
#endif
}

void tc_toggle_reverse(int train) {
	ASSERT(1 <= train && train <= 80);
#ifdef ASCII_TC
	fputs("Reversing train" EOL, COM1);
#else
	char train_command[2] = {15, train};
	fput_buf(train_command, sizeof(train_command), COM1);
#endif
}

void tc_stop(int train) {
	ASSERT(1 <= train && train <= 80);
#ifdef ASCII_TC
	fputs("Stopping train" EOL, COM1);
#else
	char train_command[2] = {0, train};
	fput_buf(train_command, sizeof(train_command), COM1);
#endif
}

void tc_switch_switch(int sw, enum sw_direction d) {
	ASSERT((1 <= sw && sw <= 18) || (145 <= sw && sw <= 156));
#ifdef ASCII_TC
	fputs("Changing switch position" EOL, COM1);
#else
	char cmd = (d == STRAIGHT) ? 0x21 : 0x22;
	char sw_command[2] = {cmd, sw};
	fput_buf(sw_command, sizeof(sw_command), COM1);
#endif
}

void tc_deactivate_switch(void) {
#ifdef ASCII_TC
	fputs("Resetting solenoid" EOL, COM1);
#else
	fputc(0x20, COM1);
#endif
}
