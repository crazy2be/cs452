#pragma once

// TODO: later, we may want to add left/right distinction, to
// simplify dealing with the tri-state switches on the track
enum sw_direction { STRAIGHT, CURVED };

int trains_set_speed(int train, int speed);
int trains_reverse(int train);
int trains_switch(int switch_numuber, enum sw_direction d);
int start_trains(void);
