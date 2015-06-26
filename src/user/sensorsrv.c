#include "sensorsrv.h"
#include "displaysrv.h"
#include "nameserver.h"
#include "clockserver.h"
#include "calibrate/calibrate.h"
#include <assert.h>
#include <io_server.h>
#include <kernel.h>
#include <util.h>

void sensor_set(struct sensor_state *s, int num, int tripped) {
	const int word_num = num / 8;
	const int offset = 7 - num % 8;
	const int bit = 0x1 << offset;

	ASSERT(word_num <= sizeof(s->packed) / sizeof(s->packed[0]));

	unsigned mask = s->packed[word_num];
	mask = tripped ? (mask | bit) : (mask & ~bit);
	s->packed[word_num] = mask;
}

int sensor_get(const struct sensor_state *s, int num) {
	const int word_num = num / 8;
	const int offset = 7 - num % 8;
	const int bit = 0x1 << offset;

	ASSERT(word_num <= sizeof(s->packed) / sizeof(s->packed[0]));

	return s->packed[word_num] & bit;
}

// 4-byte buf (including 1 byte of null-term)
void sensor_repr(int n, char *buf) {
	const char group = 'A' + (n / 16);
	const int number = (n % 16) + 1;
	snprintf(buf, 4, "%c%d", group, number);
}

void sensor_each_new(struct sensor_state *old, struct sensor_state *new,
		sensor_new_handler cb, void *ctx) {
	for (int i = 0; i <= SENSOR_COUNT; i++) {
		int s = sensor_get(new, i);
		if (s && s != sensor_get(old, i)) {
			cb(i, ctx);
		}
	}
}

static void send_sensor_poll(void) {
#ifdef QEMU
	fputs(COM1, "Sending sensor poll"EOL);
#else
	fputc(COM1, 0x85);
#endif
}

void start_sensorsrv(void) {
	// discard sensor input stuck in the train controller from the last run
	/* delay(100); */
	/* printf("%d bytes in the buffer after the initial delay" EOL, fbuflen(COM1)); */
	/* fdump(COM1); */

	/* delay(100); */
	/* printf("%d bytes in the buffer after the second delay" EOL, fbuflen(COM1)); */

	/* fputc(COM1, 0x61); */
	/* fputc(COM1, 0x60); */

	/* int displaysrv = whois(DISPLAYSRV_NAME); */
// 	int calibratesrv = whois(CALIBRATESRV_NAME);
	struct sensor_state sensors;
	// Test just to try and crash everything.
	delay(100);
	char test[10] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00};
	memcpy(sensors.packed, test, sizeof(test));
	sensors.ticks = time();
	trains_send_sensors(sensors);
	delay(100);
	char test2[10] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00};
	memcpy(sensors.packed, test2, sizeof(test2));
	sensors.ticks = time();
	trains_send_sensors(sensors);
	delay(100);
	char test3[10] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00};
	memcpy(sensors.packed, test3, sizeof(test3));
	sensors.ticks = time();
	trains_send_sensors(sensors);
	delay(100);
	char test4[10] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00};
	memcpy(sensors.packed, test4, sizeof(test4));
	sensors.ticks = time();
	trains_send_sensors(sensors);
	for (;;) {
		send_sensor_poll();
		/* printf("%d bytes in the buffer before" EOL, fbuflen(COM1)); */
		ASSERT(fgets(COM1, (char*) &sensors, 10) >= 0);
		sensors.ticks = time();

		// notify the tasks which need to know about sensor updates
		/* printf("%d bytes in the buffer after" EOL, fbuflen(COM1)); */
		/* displaysrv_update_sensor(displaysrv, &sensors); */
		//calibrate_send_sensors(calibratesrv, &sensors);
		trains_send_sensors(sensors);
	}
	/* delay(100); */
	/* printf("%d bytes in the buffer after stop" EOL, fbuflen(COM1)); */
}

void sensorsrv(void) {
	create(HIGHER(PRIORITY_MIN, 6), start_sensorsrv);
}
