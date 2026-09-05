/* L09 Sec4 / L10 Sec2 - round robin scheduling, now skips BLOCKED and
 * TERMINATED processes so mutex/semaphore can suspend a process without
 * the scheduler picking it back up until something explicitly wakes it. */
#include "scheduler.h"

extern void switch_context(uint32_t *old_esp_store, uint32_t new_esp);

#define MAX_RUNNABLE 16
static pcb_t *queue[MAX_RUNNABLE];
static int q_count = 0;
static int q_head = 0;
static pcb_t *current = (void*)0;

void scheduler_init(void) {
    q_count = 0;
    q_head = 0;
    current = (void*)0;
}

void scheduler_add(pcb_t *p) {
    if (q_count < MAX_RUNNABLE) queue[q_count++] = p;
}

pcb_t *scheduler_current(void) { return current; }

/* Called from irq0_handler on every timer tick, and also directly via
 * "int $0x20" whenever a process voluntarily yields (mutex_lock/sem_wait
 * blocking, or thread_yield). */
void scheduler_tick(void) {
    if (q_count == 0) return;

    pcb_t *prev = current;
    pcb_t *next = (void*)0;

    int tries = 0;
    while (tries < q_count) {
        pcb_t *candidate = queue[q_head];
        q_head = (q_head + 1) % q_count;
        tries++;
        if (candidate->state == PROC_READY || candidate->state == PROC_RUNNING) {
            next = candidate;
            break;
        }
    }

    if (!next) return; /* nothing runnable right now */
    if (next == prev) return; /* only one runnable process, nothing to switch */

    if (prev && prev->state == PROC_RUNNING) prev->state = PROC_READY;
    next->state = PROC_RUNNING;
    current = next;

    switch_context(prev ? &prev->esp : (uint32_t*)0, next->esp);
}

pcb_t *scheduler_bootstrap(void) {
    if (q_count == 0) return (void*)0;
    pcb_t *first = queue[q_head];
    q_head = (q_head + 1) % q_count;
    first->state = PROC_RUNNING;
    current = first;
    return first;
}
