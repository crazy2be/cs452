#include "position.h"
#include <util.h>

int position_is_uninitialized(struct position *p) {
	return p->edge == NULL;
}

void position_reverse(struct position *p) {
	p->displacement = p->edge->dist - p->displacement;
	p->edge = p->edge->reverse;
}
