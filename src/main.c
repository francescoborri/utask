#include <stdio.h>
#include <stdlib.h>

#include "ulock.h"
#include "utask.h"

typedef struct {
    char *str;
    int num;
    ulock_t *lock;
    FILE *file;
} data_t;

void func(void *arg) {
    data_t *data = (data_t *)arg;
    tid_t tid = utask_self();

    ulock_acquire(data->lock);

    fprintf(data->file, "tid: %d\n", tid);
    printf("%s prints %d on file\n", data->str, tid);

    utask_yield();

    fprintf(data->file, "tid: %d\n", tid);
    printf("%s prints %d on file\n", data->str, tid);

    ulock_release(data->lock);

    utask_yield();

    for (int i = 0; i < data->num; i++) {
        printf("%s prints %d\n", data->str, i);
        utask_yield();
    }

    printf("%s done\n", data->str);
    utask_exit(0);
}

int main(int argc, char *argv[]) {
    FILE *file = fopen("res/log.txt", "w");
    
    ulock_t *ulock = (ulock_t *)malloc(sizeof(*ulock));
    ulock_init(ulock);

    data_t arg1 = {"t1", 5, ulock, file};
    data_t arg2 = {"t2", 5, ulock, file};
    data_t arg3 = {"t3", 5, ulock, file};

    tid_t t1 = utask_spawn(&func, &arg1);
    tid_t t2 = utask_spawn(&func, &arg2);
    tid_t t3 = utask_spawn(&func, &arg3);

    utask_join(t1, NULL);
    utask_join(t2, NULL);
    utask_join(t3, NULL);

    fclose(file);
    ulock_destroy(ulock);
    free(ulock);

    return 0;
}