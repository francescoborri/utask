#include "ulock.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "utask.h"

extern tcb_t *table[MAX_UTASKS];
extern queue_t ready_queue;

extern tcb_t *running_task;

int ulock_init(ulock_t *lock) {
    assert(lock != NULL);

    lock->locked = false;
    lock->waiting_queue = (queue_t *)malloc(sizeof(queue_t));
    queue_init(lock->waiting_queue);

    return 0;
}

int ulock_destroy(ulock_t *lock) {
    assert(lock != NULL);

    if (queue_size(lock->waiting_queue) > 0)
        return -1;

    free(lock->waiting_queue);

    return 0;
}

int ulock_acquire(ulock_t *lock) {
    assert(lock != NULL);

    while (lock->locked) {
        tcb_t *waiting_task = running_task;
        queue_enqueue(lock->waiting_queue, waiting_task->tid);
        waiting_task->state = STATE_WAITING;

        assert(!queue_is_empty(&ready_queue));

        running_task = table[queue_dequeue(&ready_queue)];
        running_task->state = STATE_RUNNING;

        swapcontext(waiting_task->ctx, running_task->ctx);
    }

    lock->locked = true;
    return 0;
}

int ulock_release(ulock_t *lock) {
    assert(lock != NULL);

    if (!lock->locked)
        return -1;

    lock->locked = false;

    if (queue_size(lock->waiting_queue) > 0) {
        tcb_t *waiting_task = table[queue_dequeue(lock->waiting_queue)];
        queue_enqueue(&ready_queue, waiting_task->tid);
        waiting_task->state = STATE_READY;
    }

    return 0;
}