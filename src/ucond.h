#include <stdio.h>

#include "utask.h"
#include "ulock.h"

#ifndef UCOND_H
#define UCOND_H

typedef struct {
    queue_t *waiting_queue;
} ucond_t;

void ucond_init(ucond_t *cond);
void ucond_destroy(ucond_t *cond);
int ucond_wait(ucond_t *cond, ulock_t *lock);
void ucond_signal(ucond_t *cond);
void ucond_broadcast(ucond_t *cond);

#endif