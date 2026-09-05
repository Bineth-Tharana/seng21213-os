#ifndef SCHEDULER_H
#define SCHEDULER_H
#include "process.h"

void   scheduler_init(void);
void   scheduler_add(pcb_t *p);
void   scheduler_tick(void);
pcb_t *scheduler_current(void);
pcb_t *scheduler_bootstrap(void);

#endif
