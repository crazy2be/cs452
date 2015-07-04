#pragma once

#include "../switch_state.h"

struct switch_state tc_init_switches(void);
void tc_set_speed(int train, int speed);
void tc_toggle_reverse(int train);
void tc_stop(int train);
void tc_switch_switch(int sw, enum sw_direction d);
void tc_switch_switches_bulk(const struct switch_state switches);
void tc_deactivate_switch(void);
