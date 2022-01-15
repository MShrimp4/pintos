#ifndef VM_MMAP_H
#define VM_MMAP_H

#include <stdbool.h>
#include "filesys/file.h"

typedef int mapid_t;

mapid_t mmap           (struct file *, void *addr);
void    munmap         (mapid_t mid);
bool    mmap_load_page (mapid_t mid, void *page);
void    mmap_close_all (void);

#endif /* VM_MMAP_H */
