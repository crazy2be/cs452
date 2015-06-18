#pragma once

#include "switch_state.h"

int trains_set_speed(int train, int speed);
int trains_reverse(int train);
int trains_switch(int switch_numuber, enum sw_direction d);
int start_trains(void);
