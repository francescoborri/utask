#include <assert.h>
#include <stdbool.h>
#include <sys/types.h>
#include <ucontext.h>

#ifndef UTASK_H
#define UTASK_H

#define ERR 1

#define MAX_UTASKS 0xFFFF
#define NO_TASK MAX_UTASKS

#define STATE_READY 0
#define STATE_RUNNING 1
#define STATE_WAITING 2
#define STATE_ZOMBIE 3

#define STATIC_UTASK_QUEUE_INITIALIZER \
    { .queue = {[0 ... MAX_UTASKS - 1] = NO_TASK}, .head = 0, .tail = 0 }

typedef u_int16_t tid_t;
typedef u_int8_t state_t;

typedef struct queue {
    tid_t queue[MAX_UTASKS];
    size_t head;
    size_t tail;
} queue_t;

typedef struct tcb {
    tid_t tid;
    state_t state;
    ucontext_t *ctx;
    queue_t *waiting_queue;
    size_t waiting_tasks;
    int exit_status;
} tcb_t;

int queue_init(queue_t *queue);
bool queue_is_empty(queue_t *queue);
bool queue_is_full(queue_t *queue);
size_t queue_size(queue_t *queue);
int queue_enqueue(queue_t *queue, tid_t tid);
tid_t queue_dequeue(queue_t *queue);

tid_t new_tid();
int free_tid(tid_t tid);

tid_t utask_self();
tid_t utask_spawn(void (*routine)(void *arg), void *arg);
int utask_yield();
int utask_join(tid_t tid, int *exit_status);
void utask_exit(int exit_status);

#endif