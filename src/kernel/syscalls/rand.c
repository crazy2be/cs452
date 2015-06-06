#include "rand.h"

#include <prng.h>
#include "../tasks.h"

static struct prng random_gen;

void rand_init(unsigned seed) {
	prng_init(&random_gen, seed);
}

void rand_handler(struct task_descriptor *current_task) {
	current_task->context->r0 = prng_gen(&random_gen);
	task_schedule(current_task);
}
