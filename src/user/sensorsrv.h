#pragma once

// sensors are packed so it's cheap to message pass this struct around
struct sensor_state {
	unsigned char packed[10];
};

void sensorsrv(void);
