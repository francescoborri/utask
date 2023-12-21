#include "ucond.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "ulock.h"
#include "utask.h"

extern tcb_t *table[MAX_UTASKS];
extern queue_t ready_queue;

extern tcb_t *running_task;

void ucond_init(ucond_t *ucond) {
    assert(ucond != NULL);

    ucond->waiting_queue = (queue_t *)malloc(sizeof(queue_t));
    queue_init(ucond->waiting_queue);
}

void ucond_destroy(ucond_t *ucond) {
    assert(ucond != NULL);

    free(ucond->waiting_queue);
}

int ucond_wait(ucond_t *ucond, ulock_t *ulock) {
    assert(ucond != NULL);
    assert(ulock != NULL);

    if (queue_enqueue(ucond->waiting_queue, running_task->tid) == -1)
        return -1;

    running_task->state = STATE_WAITING;
    ulock_release(ulock);

    utask_yield();

    ulock_acquire(ulock);
    return 0;
}

void ucond_signal(ucond_t *ucond) {
    assert(ucond != NULL);

    if (queue_is_empty(ucond->waiting_queue))
        return;

    tcb_t *waiting_task = table[queue_dequeue(ucond->waiting_queue)];
    waiting_task->state = STATE_READY;
    queue_enqueue(&ready_queue, waiting_task->tid);
}

void ucond_broadcast(ucond_t *ucond) {
    assert(ucond != NULL);

    while (!queue_is_empty(ucond->waiting_queue)) {
        tcb_t *waiting_task = table[queue_dequeue(ucond->waiting_queue)];
        waiting_task->state = STATE_READY;
        queue_enqueue(&ready_queue, waiting_task->tid);
    }
}