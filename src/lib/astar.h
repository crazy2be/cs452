#include "../user/track.h"

struct astar_node {
	const struct track_node *node;
};

#define ASTAR_MAX_PATH 100

int astar_find_path(const struct track_node *start, const struct track_node *end,
					struct astar_node (*path_out)[ASTAR_MAX_PATH]);
