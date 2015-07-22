#include "astar.h"

#include <util.h>
#include <min_heap.h>

#define idx TRACK_NODE_INDEX

static int h(const struct track_node *start, const struct track_node *end) {
	int dx = start->coord_x - end->coord_x, dy = start->coord_y - end->coord_y;
	return sqrti(dx*dx + dy*dy);
}

static int reconstruct_path(int *node_parents, int start, struct astar_node *path_out) {
	int l = 0;
	int cur = start;
	while (cur >= 0) {
		ASSERT(cur >= 0 && cur < TRACK_MAX);
		cur = node_parents[cur];
		l++;
	}
	ASSERT(l <= ASTAR_MAX_PATH);
	int i = 0;
	cur = start;
	while (cur >= 0) {
		int j = l - i - 1;
		ASSERT(j >= 0 && j < TRACK_MAX);
		path_out[j].node = &track[cur];
		ASSERT(cur >= 0 && cur < TRACK_MAX);
		cur = node_parents[cur];
		i++;
	}
	return l;
}

int astar_find_path(const struct track_node *start, const struct track_node *end,
					struct astar_node *path_out) {
	memset(path_out, 0, sizeof(path_out)*ASTAR_MAX_PATH);
	printf("sizeof(*path_out), %d"EOL, sizeof(*path_out));
	struct min_heap mh;
	min_heap_init(&mh);
	min_heap_push(&mh, 0, idx(start));
	int node_g[TRACK_MAX] = {[0 ... TRACK_MAX-1] = 0x7FFFFFFF};
	int node_f[TRACK_MAX] = {[0 ... TRACK_MAX-1] = 0x7FFFFFFF};
	node_g[idx(start)] = 0;
	node_f[idx(start)] = 0;
	int node_parents[TRACK_MAX] = {[0 ... TRACK_MAX-1] = -1};

	while (!min_heap_empty(&mh)) {
		int min_i = min_heap_pop(&mh);
		ASSERT(min_i >= 0 && min_i < TRACK_MAX);
		const struct track_node *q = &track[min_i];
		ASSERT(idx(q) == min_i);
		for (int i = 0; i < 2; i++) {
			const struct track_node *suc = q->edge[i].dest;
			if (!suc) continue;
			int suc_g = node_g[idx(q)] + q->edge[i].dist;
			int suc_f = suc_g + h(suc, end);
			if (node_f[idx(suc)] < suc_f) continue;
			node_g[idx(suc)] = suc_g;
			node_f[idx(suc)] = suc_f;
			node_parents[idx(suc)] = idx(q);
			if (suc == end) {
				return reconstruct_path(node_parents, idx(suc), path_out);
			}
			min_heap_push(&mh, suc_f, idx(suc));
		}
	}
	return -1;
}

void astar_print_path(struct astar_node *path, int l) {
	ASSERT(l >= 0);
	if (l == 0) {
		printf("[Empty path]"EOL);
		return;
	}
	ASSERT(l <= ASTAR_MAX_PATH);
	printf("%p %x %s", path[0].node, idx(path[0].node), path[0].node->name);
	for (int i = 1; i < l; i++) {
		printf(" -> %p %x %s", path[i].node, idx(path[i].node), path[i].node->name);
	}
	printf(EOL);
}
