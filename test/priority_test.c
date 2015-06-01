#include <priority.h>

#include <stdio.h>
#include <stdlib.h>

void print_priority(struct priority_queue *q) {
    unsigned i;
    printf("[ ");
    for (i = 0; i < q->size; i++) {
        printf("(%d, %d) ", q->buf[i].val, q->buf[i].priority);
    }
    printf("]\n");
}

int priority_invalid(struct priority_queue *q) {
    unsigned i, child, offset;
    for (i = 0; i < q->size; i++) {
        child = i * 2 + 1;
        for (offset = 0; offset < 2; offset++) {
            if (child + offset < q->size && q->buf[i].priority < q->buf[child + offset].priority) {
                return 1;
            }
        }
    }
    return 0;
}

int test_priority_insert(void) {
    int i, v, last_priority;
    struct priority_queue q;
    int sz = PRIORITY_QUEUE_SIZE;
    int prio[sz];

    queue_init(&q);

    for (i = 0; i < sz; i++) {
        prio[i] = rand();
        if (queue_push(&q, i, prio[i])) {
            printf("Failed to insert into priority queue\n");
            return 1;
        } else if (q.size != i + 1) {
            printf("After insert, size was unexpected: %d != %d\n", q.size, i + 1);
            return 1;
        } else if (priority_invalid(&q)) {
            printf("After insert, queue became invalid\n");
        }
    }

    /* print_priority(&q); */

    if (queue_pop(&q, &v)) {
        printf("Failed to pop from priority queue (%d)\n", i);
        return 1;
    } else if (q.size != i - 1) {
        printf("After pop, size was unexpected: %d != %d\n", q.size, i - 1);
        return 1;
    } else if (priority_invalid(&q)) {
        printf("After pop, queue became invalid\n");
    }
    i--;

    last_priority = prio[v];

    for (; i > 0; i--) {
        if (queue_pop(&q, &v)) {
            printf("Failed to pop from priority queue (%d)\n", i);
            return 1;
        } else if (q.size != i - 1) {
            printf("After pop, size was unexpected: %d != %d\n", q.size, i - 1);
            return 1;
        } else if (priority_invalid(&q)) {
            printf("After pop, queue became invalid\n");
        } else if (last_priority < prio[v]) {
            printf("Misordered priority popped from queue, %d < %d\n", last_priority, prio[v]);
            return 1;
        }
        last_priority = prio[v];
    }
    return 0;
}

int test_priority(void) {
    return test_priority_insert();
}

int main(void) {
    int failed = test_priority();
    printf("suite priority failed %d tests\n", failed);
    return 0;
}
