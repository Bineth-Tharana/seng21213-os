/* L09 Sec2 - process table and PCB creation */
#include "process.h"

static pcb_t proc_table[MAX_PROCESSES];
static uint32_t next_pid = 1;
static uint8_t proc_stacks[MAX_PROCESSES][PROC_STACK_SIZE];

void process_init(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) proc_table[i].state = PROC_UNUSED;
}

static void p_strncpy(char *dst, const char *src, int n) {
    int i = 0;
    for (; i < n - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

/* Builds a fake stack frame matching what switch_context's own
 * "pushad" would leave behind, with entry_fn as the return address
 * below it. This means the very first switch_context() call into a
 * brand-new process just falls straight into entry_fn via RET - no
 * fake interrupt frame required. */
pcb_t *create_process(void (*entry_fn)(void), const char *name) {
    int slot = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].state == PROC_UNUSED) { slot = i; break; }
    }
    if (slot == -1) return (void*)0;

    pcb_t *p = &proc_table[slot];
    p->pid = next_pid++;
    p->state = PROC_READY;
    p_strncpy(p->name, name, sizeof(p->name));

    uint32_t stack_top = (uint32_t)&proc_stacks[slot][PROC_STACK_SIZE];
    uint32_t *sp = (uint32_t*)stack_top;

    *(--sp) = (uint32_t)(void*)entry_fn; /* fake return address for RET */
    *(--sp) = 0; /* eax */
    *(--sp) = 0; /* ecx */
    *(--sp) = 0; /* edx */
    *(--sp) = 0; /* ebx */
    *(--sp) = 0; /* esp (dummy, ignored by popad) */
    *(--sp) = 0; /* ebp */
    *(--sp) = 0; /* esi */
    *(--sp) = 0; /* edi  <- final esp lands here, matching popad's pop order */

    p->esp = (uint32_t)sp;
    return p;
}

pcb_t *process_table_entry(int index) {
    if (index < 0 || index >= MAX_PROCESSES) return (void*)0;
    return &proc_table[index];
}
