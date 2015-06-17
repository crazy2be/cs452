#include "sensorsrv.h"
#include "displaysrv.h"
#include "nameserver.h"
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
	// this is a hack, since I don't want to implement general sprintf right now
	const int group = n / 16;
	int number = (n % 16) + 1;
	*buf++ = 'A' + group;
	if (number >= 10) {
		*buf++ = '1';
		number -= 10;
	}
	*buf++ = '0' + number;
	*buf++ = '\0';
}

static void send_sensor_poll(void) {
	fputc(COM1, 0x85);
}

void start_sensorsrv(void) {
	int displaysrv = whois(DISPLAYSRV_NAME);
	struct sensor_state sensors;
	for (;;) {
		send_sensor_poll();
		ASSERT(fgets(COM1, (char*) &sensors, 10) >= 0);
		displaysrv_update_sensor(displaysrv, &sensors);
	}
}

void sensorsrv(void) {
	create(HIGHER(PRIORITY_MIN, 6), start_sensorsrv);
}
