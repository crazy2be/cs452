#include "astar.h"

#include <util.h>
#include <min_heap.h>

static int h(const struct track_node *start, const struct track_node *end) {
	int dx = start->coord_x - end->coord_x, dy = start->coord_y - end->coord_y;
	return sqrti(dx*dx + dy*dy);
}

static int reconstruct_path(const struct track_node *node_parents[TRACK_MAX],
							 const struct track_node *current,
							 struct astar_node (*path_out)[ASTAR_MAX_PATH]) {
	int l = 0;
	const struct track_node *cur = current;
	while (cur != NULL) {
		cur = node_parents[TRACK_NODE_INDEX(cur)];
		l++;
	}
	ASSERT(l < ASTAR_MAX_PATH);
	int i = 0;
	cur = current;
	while (cur != NULL) {
		path_out[l - i - 1]->node = cur;
		cur = node_parents[TRACK_NODE_INDEX(cur)];
		i++;
	}
	return l;
}

int astar_find_path(const struct track_node *start, const struct track_node *end,
					struct astar_node (*path_out)[ASTAR_MAX_PATH]) {
	memset(path_out, -1, sizeof(*path_out));
	struct min_heap mh;
	min_heap_init(&mh);
	min_heap_push(&mh, 0, TRACK_NODE_INDEX(start));
	int node_g[TRACK_MAX] = {[0 ... TRACK_MAX-1] = 0x7FFFFFFF};
	int node_f[TRACK_MAX] = {[0 ... TRACK_MAX-1] = 0x7FFFFFFF};
	node_g[TRACK_NODE_INDEX(start)] = 0;
	node_f[TRACK_NODE_INDEX(start)] = 0;
	const struct track_node *node_parents[TRACK_MAX] = {};

	while (!min_heap_empty(&mh)) {
		int min_i = min_heap_pop(&mh);
		const struct track_node *q = &track[min_i];
		for (int i = 0; i < 2; i++) {
			const struct track_node *suc = q->edge[i].dest;
			if (!suc) continue;
			int suc_g = node_g[TRACK_NODE_INDEX(q)] + q->edge[i].dist;
			int suc_f = suc_g + h(suc, end);
			if (node_f[TRACK_NODE_INDEX(suc)] < suc_f) continue;
			node_g[TRACK_NODE_INDEX(suc)] = suc_g;
			node_f[TRACK_NODE_INDEX(suc)] = suc_f;
			node_parents[TRACK_NODE_INDEX(suc)] = q;
			if (suc == end) {
				return reconstruct_path(node_parents, suc, path_out);
			}
			min_heap_push(&mh, suc_f, TRACK_NODE_INDEX(suc));
		}
	}
	return -1;
}

void astar_print_path(struct astar_node (*path)[ASTAR_MAX_PATH], int l) {
	ASSERT(l >= 0);
	if (l == 0) {
		printf("[Empty path]"EOL);
		return;
	}
	ASSERT(l <= ASTAR_MAX_PATH);
	printf("%p %s", path[0]->node, path[0]->node->name);
	for (int i = 1; i < l; i++) {
		printf(" -> %p %s", path[i]->node, path[i]->node->name);
	}
	printf(EOL);
}
