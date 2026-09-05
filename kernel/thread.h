#ifndef THREAD_H
#define THREAD_H
#include "process.h"

typedef pcb_t thread_t;

thread_t *thread_create(void (*fn)(void *arg), void *arg);
void      thread_yield(void);

#endif
