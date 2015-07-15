#pragma once

#include "trainsrv.h"
#include "sensorsrv.h"

#define DISPLAYSRV_NAME "displaysrv"

// spawns a new display server task
void displaysrv(void);
void displaysrv_update_switch(int displaysrv, struct switch_state *state);
void displaysrv_update_sensor(int displaysrv, struct sensor_state *state, unsigned avg_delay);
void displaysrv_console_clear(int displaysrv);
void displaysrv_console_backspace(int displaysrv);
void displaysrv_console_input(int displaysrv, char c);
void displaysrv_console_feedback(int displaysrv, char *fb);
void displaysrv_quit(int displaysrv);
