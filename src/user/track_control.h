#pragma once

// TODO: later, we may want to add left/right distinction, to
// simplify dealing with the tri-state switches on the track
enum sw_direction { STRAIGHT, CURVED };

void set_train_speed(int train, int speed);
void set_switch_state(int sw, enum sw_direction d);
void disable_switch_solenoid(void);
void send_sensor_poll(void);
