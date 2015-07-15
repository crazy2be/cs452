#pragma once
#include <util.h>
#include "../trainsrv.h"

void train_alert_start(struct switch_state switches, bool actually_delay);

void train_alert_update_train(int train_id, struct position position);
void train_alert_update_train_speed(int train_id);
void train_alert_update_switch(struct switch_state switches);
int train_alert_at(int train_id, struct position position);
