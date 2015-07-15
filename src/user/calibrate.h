#pragma once
#include "sensorsrv.h"
#include "switch_state.h"

#define CALIBRATESRV_NAME "calibratesrv"

void calibratesrv(void);
void calibrate_send_switches(int calibratesrv, struct switch_state *st);
void calibrate_send_sensors(int calibratesrv, struct sensor_state *st);
