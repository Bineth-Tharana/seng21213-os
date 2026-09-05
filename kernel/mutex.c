/* L10 Sec2 - blocking mutex. Uses atomic exchange for the fast path;
 * on contention it BLOCKS the calling process (removes it from
 * scheduling) instead of spinning, which is what makes this a real
 * mutex rather than a spinlock. */
#include "mutex.h"
#include "scheduler.h"

void mutex_init(mutex_t *m) {
    m->locked = 0;
    m->waiter_count = 0;
}

static int test_and_set(volatile int *lock) {
    int old;
    __asm__ volatile (
        "lock xchg %0, %1"
        : "=r"(old), "+m"(*lock)
        : "0"(1)
        : "memory"
    );
    return old;
}

void mutex_lock(mutex_t *m) {
    while (test_and_set(&m->locked)) {
        pcb_t *self = scheduler_current();
        self->state = PROC_BLOCKED;
        if (m->waiter_count < 8) m->waiters[m->waiter_count++] = self;
        __asm__ volatile ("int $0x20"); /* yield - resumes here once woken */
    }
}

void mutex_unlock(mutex_t *m) {
    m->locked = 0;
    if (m->waiter_count > 0) {
        pcb_t *w = m->waiters[0];
        for (int i = 1; i < m->waiter_count; i++) m->waiters[i - 1] = m->waiters[i];
        m->waiter_count--;
        if (w->state == PROC_BLOCKED) w->state = PROC_READY;
    }
}
