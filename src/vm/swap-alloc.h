#ifndef VM_SWAP_ALLOC_H
#define VM_SWAP_ALLOC_H

#include <stddef.h>
#include <stdbool.h>

void   swap_init (void);

size_t swap_store_page     (const void *);
size_t swap_store_multiple (const void *, size_t page_cnt);
void   swap_load_page      (size_t, void *);
void   swap_load_multiple  (size_t, void *, size_t page_cnt);

bool   swap_is_valid (size_t);

#endif /* vm/swap-alloc.h */
