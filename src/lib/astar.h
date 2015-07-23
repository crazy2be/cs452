#include "../user/track.h"

struct astar_node {
	const struct track_node *node;
};

#define ASTAR_MAX_PATH 100

int astar_find_path(const struct track_node *start, const struct track_node *end,
					struct astar_node *path_out);

void astar_print_path(struct astar_node *path, int l);
