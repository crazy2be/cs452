#include "astar.h"

#include <util.h>
#include <min_heap.h>

static int h(struct track_node *start, struct track_node *end) {
	int dx = start->coord_x - end->coord_x, dy = start->coord_y - end->coord_y;
	return sqrti(dx*dx + dy*dy);
}

static void reconstruct_path(struct track_node *node_parents[TRACK_MAX],
							 struct track_node *current,
							 struct astar_node (*path_out)[ASTAR_MAX_PATH]) {
	int l = 0;
	track_node *cur = current;
	while (cur != NULL) {
		cur = node_parents[TRACK_NODE_INDEX(cur)];
		l++;
	}
	ASSERT(l < ASTAR_MAX_PATH);
	int i = 0;
	cur = current;
	while (cur != NULL) {
		(*path_out)[l - i].node = cur;
		cur = node_parents[TRACK_NODE_INDEX(cur)];
		i++;
	}
// 	path = []
// 	while current is not None:
// 		if current in path:
// 			raise Exception("cycle at %d" % current.i)
// 		path.insert(0, current)
// 		current = node_parents[current.i]
// 	return path
}

void astar_find_path(struct track_node *start, struct track_node *end,
					 struct astar_node (*path_out)[ASTAR_MAX_PATH]) {
	struct min_heap mh;
	min_heap_init(&mh);
	min_heap_push(&mh, 0, TRACK_NODE_INDEX(start));
	int node_g[TRACK_MAX];
	int node_f[TRACK_MAX];
	node_g[TRACK_NODE_INDEX(start)] = 0;
	node_f[TRACK_NODE_INDEX(start)] = 0;
	track_node *node_parents[TRACK_MAX];

	while (!min_heap_empty(&mh)) {
		int min_i = min_heap_pop(&mh);
		track_node *q = &track[min_i];
		for (int i = 0; i < 2; i++) {
			track_node *suc = q->edge[i].dest;
			if (!suc) continue;
			int suc_g = node_g[TRACK_NODE_INDEX(q)] + q->edge[i].dist;
			int suc_f = suc_g + h(suc, end);
			if (node_f[TRACK_NODE_INDEX(suc)] < suc_f) continue;
			node_g[TRACK_NODE_INDEX(suc)] = suc_g;
			node_f[TRACK_NODE_INDEX(suc)] = suc_f;
			node_parents[TRACK_NODE_INDEX(suc)] = q;
			if (suc == end) {
				reconstruct_path(node_parents, suc, path_out);
				return;
			}
			min_heap_push(&mh, suc_f, TRACK_NODE_INDEX(suc));
		}
	}
// 		mh = []
// 		min_heap_push(mh, 0., start.i)
// 		node_g = [100000000.]*len(cur_track)
// 		node_f = [100000000.]*len(cur_track)
// 		node_g[start.i], node_f[start.i] = 0., 0.
// 		node_parents = [None]*len(cur_track)
//
// 		while len(mh) > 0:
// 			min_i = min_heap_pop(mh)
// 			q = cur_track[min_i]
// 			for suc_edge in q.edge:
// 				suc = suc_edge.dest
// 				if suc is None:
// 					continue
// 				suc_g = node_g[q.i] + suc_edge.dist
// 				suc_f = suc_g + h(suc, end)
// 				if node_f[suc.i] < suc_f:
// 					continue
// 				node_g[suc.i], node_f[suc.i] = suc_g, suc_f
// 				node_parents[suc.i] = q
// 				if suc == end:
// 					return reconstruct_path(node_parents, suc)
// 				min_heap_push(mh, suc_f, suc.i)
// 		return None
}
