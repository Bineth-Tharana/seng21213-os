/* L09 Sec4 - round robin scheduling, driven by IRQ0 (see idt.c) */
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

/* Called from irq0_handler on every timer tick (100Hz). */
void scheduler_tick(void) {
    if (q_count == 0) return;

    pcb_t *prev = current;
    pcb_t *next = queue[q_head];
    q_head = (q_head + 1) % q_count;

    if (next == prev) return; /* only one runnable process, nothing to do */

    if (prev && prev->state == PROC_RUNNING) prev->state = PROC_READY;
    next->state = PROC_RUNNING;
    current = next;

    switch_context(prev ? &prev->esp : (uint32_t*)0, next->esp);
}

/* Called once from kernel_main to kick off the very first process. */
pcb_t *scheduler_bootstrap(void) {
    if (q_count == 0) return (void*)0;
    pcb_t *first = queue[q_head];
    q_head = (q_head + 1) % q_count;
    first->state = PROC_RUNNING;
    current = first;
    return first;
}
