#include <stdbool.h>

#include "utask.h"

#ifndef ULOCK_H
#define ULOCK_H

typedef struct ulock {
    bool locked;
    queue_t *waiting_queue;
} ulock_t;

int ulock_init(ulock_t *lock);
int ulock_destroy(ulock_t *lock);
int ulock_acquire(ulock_t *lock);
int ulock_release(ulock_t *lock);

#endif