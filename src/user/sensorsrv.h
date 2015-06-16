#pragma once

// sensors are packed so it's cheap to message pass this struct around
struct sensor_state {
	unsigned char packed[10];
};
int sensor_get(struct sensor_state *s, int num);
void sensor_set(struct sensor_state *s, int num, int tripped);
void sensor_repr(int n, char *buf);

void sensorsrv(void);
