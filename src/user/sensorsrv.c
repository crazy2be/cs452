#include "sensorsrv.h"
#include "track_control.h"
#include <assert.h>
#include <io_server.h>
#include <kernel.h>
#include <util.h>

void sensor_set(struct sensor_state *s, int num, int tripped) {
	const int word_num = num / 8;
	const int offset = num % 8;
	const int bit = 0x1 << offset;

	ASSERT(word_num <= sizeof(s->packed) / sizeof(s->packed[0]));

	unsigned mask = s->packed[word_num];
	mask = tripped ? (mask | bit) : (mask & ~bit);
	s->packed[word_num] = mask;
}

int sensor_get(struct sensor_state *s, int num) {
	const int word_num = num / 8;
	const int offset = num % 8;
	const int bit = 0x1 << offset;

	ASSERT(word_num <= sizeof(s->packed) / sizeof(s->packed[0]));

	return s->packed[word_num] & bit;
}

void start_sensorsrv(void) {
	struct sensor_state sensors, old_sensors;

	memset(&old_sensors, 0, sizeof(old_sensors));

	for (;;) {
		send_sensor_poll();
		fgets(COM1, (char*) &sensors, 10);

		for (int i = 0; i < 80; i++) {
			int status = sensor_get(&sensors, i);
			if (status != sensor_get(&old_sensors, i)) {
				printf("Saw sensor %d change to %d" EOL, i, status);
			}
		}

		old_sensors = sensors;
	}
}

void sensorsrv(void) {
	create(HIGHER(PRIORITY_MIN, 6), start_sensorsrv);
}
