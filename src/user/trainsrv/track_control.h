#pragma once

#include "../switch_state.h"

void tc_set_speed(int train, int speed);
void tc_toggle_reverse(int train);
void tc_stop(int train);
void tc_switch_switch(int sw, enum sw_direction d);
void tc_deactivate_switch(void);
