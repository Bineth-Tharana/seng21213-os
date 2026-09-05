/* L10 Sec3 - counting semaphore, same block/wake mechanism as mutex.c */
#include "semaphore.h"
#include "scheduler.h"

void sem_init(semaphore_t *s, int initial) {
    s->count = initial;
    s->waiter_count = 0;
}

void sem_wait(semaphore_t *s) {
    __asm__ volatile ("cli");
    s->count--;
    if (s->count < 0) {
        pcb_t *self = scheduler_current();
        self->state = PROC_BLOCKED;
        if (s->waiter_count < 8) s->waiters[s->waiter_count++] = self;
        __asm__ volatile ("sti");
        __asm__ volatile ("int $0x20");
    } else {
        __asm__ volatile ("sti");
    }
}

void sem_signal(semaphore_t *s) {
    __asm__ volatile ("cli");
    s->count++;
    if (s->waiter_count > 0) {
        pcb_t *w = s->waiters[0];
        for (int i = 1; i < s->waiter_count; i++) s->waiters[i - 1] = s->waiters[i];
        s->waiter_count--;
        if (w->state == PROC_BLOCKED) w->state = PROC_READY;
    }
    __asm__ volatile ("sti");
}
