#pragma once

#include "track.h"

#include <astar.h>

void tracksrv_start(void);

// For debugging: associate a train_id with this conductor_id.
void tracksrv_set_train_id(int train_id);

void tracksrv_get_reservation_table(int *table_out);

// Reserves all of the track along path for the train associated with this
// conductor. Also reserves at least stopping_distance track at branch nodes
// (TODO), and releases any track held by this train that is not along the
// given path or branches.
// This operation is atomic. Either all the track will be reserved, and the
// total number of reserved segments is returned, or none of it will, and an
// error code < 0 will be returned.
int tracksrv_reserve_path(struct astar_node *path, int len, int stopping_distance);

// desired is a desired bitmap of size TRACK_MAX. Replaces previous reservations
// associated with this tid. Returns number of reserved segments on success,
// -number of conflicts on failure.
int tracksrv_reserve(int tid, bool *desired);

// For tests only
int tracksrv_reserve_path_test(struct astar_node *path, int len, int stopping_distance, int tid);
