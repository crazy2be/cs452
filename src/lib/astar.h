#include "../user/track.h"

struct astar_node {
	track_node *node;
};

#define ASTAR_MAX_PATH 100

void astar_find_path(struct track_node *start, struct track_node *end,
					 struct astar_node (*path_out)[ASTAR_MAX_PATH]);
