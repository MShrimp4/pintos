#ifndef THREADS_MALLOC_H
#define THREADS_MALLOC_H

#include <debug.h>
#include <stddef.h>

void malloc_init (void);
void free (void *);
void *malloc (size_t) MALLOC(free);
void *calloc (size_t, size_t) MALLOC(free);
void *realloc (void *, size_t);

#endif /* threads/malloc.h */
