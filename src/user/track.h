#pragma once

#include <util.h>

#include "calibrate/track_data_new.h"

extern struct track_node track[TRACK_MAX];

const struct track_node* track_node_from_sensor(int sensor);
