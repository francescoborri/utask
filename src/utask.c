#include "utask.h"

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/time.h>

tcb_t *table[MAX_UTASKS] = {[0 ... MAX_UTASKS - 1] = NULL};
queue_t ready_queue = STATIC_UTASK_QUEUE_INITIALIZER;
queue_t free_tids = STATIC_UTASK_QUEUE_INITIALIZER;

ucontext_t main_ctx;
queue_t main_waiting_queue = STATIC_UTASK_QUEUE_INITIALIZER;
tcb_t main_task = {.tid = 0, .state = STATE_RUNNING, .ctx = &main_ctx, .waiting_queue = &main_waiting_queue, .waiting_tasks = 0};

tcb_t *running_task = NULL;

int queue_init(queue_t *queue) {
    if (queue == NULL)
        return (errno = EINVAL, ERR);

    *queue = (queue_t)STATIC_UTASK_QUEUE_INITIALIZER;

    return 0;
}

bool queue_is_empty(queue_t *queue) {
    return queue->head == queue->tail && queue->queue[queue->head] == NO_TASK;
}

bool queue_is_full(queue_t *queue) {
    return queue->head == queue->tail && queue->queue[queue->head] != NO_TASK;
}

size_t queue_size(queue_t *queue) {
    if (queue == NULL)
        return (errno = EINVAL, ERR);

    if (queue->head == queue->tail) {
        if (queue_is_empty(queue))
            return 0;
        else
            return MAX_UTASKS;
    }

    return queue->head < queue->tail ? queue->tail - queue->head : MAX_UTASKS - queue->head + queue->tail;
}

int queue_enqueue(queue_t *queue, tid_t tid) {
    if (queue == NULL || tid == NO_TASK)
        return (errno = EINVAL, ERR);

    if (queue_is_full(queue))
        return (errno = ENOMEM, ERR);

    assert(queue->queue[queue->tail] == NO_TASK);

    queue->queue[queue->tail] = tid;
    queue->tail = (queue->tail + 1) % MAX_UTASKS;

    return 0;
}

tid_t queue_dequeue(queue_t *queue) {
    if (queue_is_empty(queue))
        return (errno = EAGAIN, NO_TASK);

    tid_t tid = queue->queue[queue->head];
    assert(tid != NO_TASK);

    queue->queue[queue->head] = NO_TASK;
    queue->head = (queue->head + 1) % MAX_UTASKS;

    return tid;
}

tid_t new_tid() {
    if (queue_is_empty(&free_tids))
        return (errno = ENOMEM, NO_TASK);

    tid_t tid = queue_dequeue(&free_tids);
    if (tid == NO_TASK)
        return (errno = ENOMEM, NO_TASK);

    return tid;
}

int free_tid(tid_t tid) {
    if (tid == NO_TASK)
        return (errno = EINVAL, ERR);

    if (queue_enqueue(&free_tids, tid) == ERR)
        return (errno = ENOMEM, ERR);

    table[tid] = NULL;

    return 0;
}

tid_t utask_self() {
    if (running_task == NULL)
        return (errno = EAGAIN, NO_TASK);

    return running_task->tid;
}

tid_t utask_spawn(void (*routine)(void *arg), void *arg) {
    static struct rlimit rlim;

    if (running_task == NULL) {
        getrlimit(RLIMIT_STACK, &rlim);

        for (tid_t tid = 0; tid < MAX_UTASKS; tid++)
            queue_enqueue(&free_tids, tid);

        main_task.tid = new_tid();

        running_task = &main_task;
        main_task.state = STATE_RUNNING;

        table[main_task.tid] = &main_task;
    }

    tid_t tid = new_tid();
    if (tid == NO_TASK)
        return (errno = ENOMEM, NO_TASK);

    assert(table[tid] == NULL);  // debug

    tcb_t *tcb = (tcb_t *)malloc(sizeof(*tcb));
    tcb->tid = tid;

    queue_enqueue(&ready_queue, tcb->tid);
    tcb->state = STATE_READY;

    tcb->ctx = (ucontext_t *)malloc(sizeof(*tcb->ctx));

    if (getcontext(tcb->ctx) == ERR)
        return NO_TASK;

    tcb->ctx->uc_stack.ss_sp = malloc(rlim.rlim_cur);
    tcb->ctx->uc_stack.ss_size = rlim.rlim_cur;
    tcb->ctx->uc_link = (ucontext_t *)malloc(sizeof(*tcb->ctx->uc_link));

    makecontext(tcb->ctx, (void (*)(void))routine, 1, arg);

    tcb->waiting_queue = (queue_t *)malloc(sizeof(queue_t));
    queue_init(tcb->waiting_queue);

    tcb->waiting_tasks = 0;

    table[tcb->tid] = tcb;

    return tcb->tid;
}

int utask_yield() {
    if (queue_is_empty(&ready_queue))
        return -1;

    tcb_t *leaving_task = running_task;
    tcb_t *next_task = table[queue_dequeue(&ready_queue)];

    queue_enqueue(&ready_queue, leaving_task->tid);
    leaving_task->state = STATE_READY;

    running_task = next_task;
    next_task->state = STATE_RUNNING;

    swapcontext(leaving_task->ctx, next_task->ctx);

    return 0;
}

int utask_join(tid_t tid, int *exit_status) {
    assert(tid >= 0 && tid < MAX_UTASKS);
    assert(table[tid] != NULL);

    if (tid == running_task->tid)
        return -1;

    tcb_t *waiting_task = running_task;
    tcb_t *waited_task = table[tid];

    assert(waited_task != NULL);

    waited_task->waiting_tasks++;

    if (waited_task->state != STATE_ZOMBIE) {
        assert(!queue_is_empty(&ready_queue));

        queue_enqueue(waited_task->waiting_queue, waiting_task->tid);
        waiting_task->state = STATE_WAITING;

        running_task = table[queue_dequeue(&ready_queue)];
        running_task->state = STATE_RUNNING;

        swapcontext(waiting_task->ctx, running_task->ctx);
    }

    if (exit_status != NULL)
        *exit_status = waited_task->exit_status;

    // !!! might generate a segmentation fault
    if (--waited_task->waiting_tasks == 0) {
        free_tid(tid);

        free(waited_task->ctx->uc_stack.ss_sp);
        free(waited_task->ctx->uc_link);
        free(waited_task->ctx);
        free(waited_task->waiting_queue);
        free(waited_task);
    }

    return 0;
}

void dumb() {}

void utask_exit(int exit_status) {
    assert(running_task->tid != 0);

    tcb_t *ending_task = running_task;

    assert(ending_task->waiting_tasks == queue_size(ending_task->waiting_queue));

    ending_task->exit_status = exit_status;
    ending_task->state = STATE_ZOMBIE;

    while (!queue_is_empty(ending_task->waiting_queue)) {
        tcb_t *waiting_task = table[queue_dequeue(ending_task->waiting_queue)];
        queue_enqueue(&ready_queue, waiting_task->tid);
        waiting_task->state = STATE_READY;
    }

    assert(!queue_is_empty(&ready_queue));

    running_task = table[queue_dequeue(&ready_queue)];
    running_task->state = STATE_RUNNING;

    getcontext(ending_task->ctx->uc_link);
    ending_task->ctx->uc_link->uc_stack.ss_sp = ending_task->ctx->uc_stack.ss_sp;
    ending_task->ctx->uc_link->uc_stack.ss_size = ending_task->ctx->uc_stack.ss_size;
    ending_task->ctx->uc_link->uc_link = running_task->ctx;
    makecontext(ending_task->ctx->uc_link, &dumb, 0);
}