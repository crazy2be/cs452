#include "sensorsrv.h"
#include "displaysrv.h"
#include "sys.h"
#include "calibrate.h"
#include <assert.h>
#include <io.h>
#include <kernel.h>
#include <util.h>

#include "../kernel/drivers/timer.h"

void sensor_set(struct sensor_state *s, int num, int tripped) {
	const int word_num = num / 8;
	const int offset = 7 - num % 8;
	const int bit = 0x1 << offset;

	ASSERTF(word_num < ARRAY_LENGTH(s->packed), "%d %d", word_num, ARRAY_LENGTH(s->packed));
	unsigned mask = s->packed[word_num];
	mask = tripped ? (mask | bit) : (mask & ~bit);
	s->packed[word_num] = mask;
}

int sensor_get(const struct sensor_state *s, int num) {
	const int word_num = num / 8;
	const int offset = 7 - num % 8;
	const int bit = 0x1 << offset;

	ASSERTF(word_num < ARRAY_LENGTH(s->packed), "%d %d", word_num, ARRAY_LENGTH(s->packed));
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
	for (int i = 0; i < SENSOR_COUNT; i++) {
		int s = sensor_get(new, i);
		if (s && s != sensor_get(old, i)) {
			cb(i, ctx);
		}
	}
}

static void tc_send_sensor_poll(void) {
#ifdef ASCII_TC
	fputs("Sending sensor poll"EOL, COM1);
#else
	fputc(0x85, COM1);
#endif
}

void start_sensorsrv(void) {
	// discard sensor input stuck in the train controller from the last run
	delay(100);
	char buf[80];
	fgetsnb(buf, sizeof(buf), COM1);

	int displaysrv = whois(DISPLAYSRV_NAME);
	struct sensor_state sensors;
#if CALIBRATE
	int calibratesrv = whois(CALIBRATESRV_NAME);
#else
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
#endif
	for (;;) {
		unsigned start_time = debug_timer_useconds();
		tc_send_sensor_poll();
		fgets((char*) &sensors, 10, COM1);
		sensors.ticks = time();

		unsigned end_time = debug_timer_useconds();
		unsigned delay_time = (end_time - start_time) / 1000;

		// notify the tasks which need to know about sensor updates
		displaysrv_update_sensor(displaysrv, &sensors, delay_time);
#if CALIBRATE
		calibrate_send_sensors(calibratesrv, &sensors);
#else
		trains_send_sensors(sensors);
#endif
	}
	/* delay(100); */
	/* printf("%d bytes in the buffer after stop" EOL, fbuflen(COM1)); */
}

void sensorsrv(void) {
	create(HIGHER(PRIORITY_MIN, 6), start_sensorsrv);
}
