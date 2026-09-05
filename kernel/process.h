#ifndef PROCESS_H
#define PROCESS_H
#include "../include/types.h"

#define MAX_PROCESSES 16
#define PROC_STACK_SIZE 4096

typedef enum {
    PROC_UNUSED = 0,
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED,
    PROC_TERMINATED
} proc_state_t;

typedef struct {
    uint32_t pid;
    proc_state_t state;
    uint32_t esp;
    char name[16];
} pcb_t;

void   process_init(void);
pcb_t *create_process(void (*entry_fn)(void), const char *name);
pcb_t *process_table_entry(int index);

#endif
