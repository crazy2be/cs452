#include "position.h"
#include <util.h>

int position_is_uninitialized(const struct position *p) {
	return p->edge == NULL;
}

void position_reverse(struct position *p) {
	p->displacement = p->edge->dist - p->displacement;
	p->edge = p->edge->reverse;
}

int position_is_wellformed(struct position *p) {
	return p->displacement >= 0 && p->displacement < p->edge->dist;
}
