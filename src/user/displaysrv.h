#pragma once

#include "track_control.h"

#define DISPLAYSRV_NAME "displaysrv"

// spawns a new display server task
void displaysrv(void);
void displaysrv_update_switch(int displaysrv, int sw, enum sw_direction pos);
void displaysrv_console_clear(int displaysrv);
void displaysrv_console_backspace(int displaysrv);
void displaysrv_console_input(int displaysrv, char c);
void displaysrv_console_feedback(int displaysrv, char *fb);
void displaysrv_quit(int displaysrv);
