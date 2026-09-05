#ifndef SEMAPHORE_H
#define SEMAPHORE_H
#include "process.h"

typedef struct {
    volatile int count;
    pcb_t *waiters[8];
    int    waiter_count;
} semaphore_t;

void sem_init(semaphore_t *s, int initial);
void sem_wait(semaphore_t *s);
void sem_signal(semaphore_t *s);

#endif
