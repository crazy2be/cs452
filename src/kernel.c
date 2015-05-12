#include <kernel.h>

// task_mem_segment: given a task id, returns the start of the segment of memory
// allocated for that task.
// It's the responsibility of the task to not overrun the end of this section.
// TODO: it may be worth changing the signature of this method to return both
// the start and the end of the segment.
void* task_mem_segment(unsigned task_id) {
    return 0;
}
