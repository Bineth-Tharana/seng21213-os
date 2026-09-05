/* L10 Sec1 - kernel threads. A thread is just a pcb_t whose entry point
 * is a trampoline that calls fn(arg); since create_process only stores a
 * bare void(*)(void) entry, the pending fn/arg pair is keyed by PID
 * (not a single global) so multiple thread_create() calls made back to
 * back before any of them actually run cannot overwrite each other. */
#include "thread.h"
#include "scheduler.h"

#define MAX_PENDING 16
static void (*g_fn[MAX_PENDING])(void*);
static void  *g_arg[MAX_PENDING];

static void thread_trampoline(void) {
    pcb_t *self = scheduler_current();
    int slot = self->pid % MAX_PENDING;
    void (*fn)(void*) = g_fn[slot];
    void *arg = g_arg[slot];

    fn(arg);

    self->state = PROC_TERMINATED;
    while (1) { __asm__ volatile ("hlt"); }
}

thread_t *thread_create(void (*fn)(void *arg), void *arg) {
    pcb_t *t = create_process(thread_trampoline, "thread");
    if (!t) return (void*)0;
    int slot = t->pid % MAX_PENDING;
    g_fn[slot] = fn;
    g_arg[slot] = arg;
    scheduler_add(t);
    return t;
}

void thread_yield(void) {
    __asm__ volatile ("int $0x20");
}
