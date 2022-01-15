#ifndef USERPROG_PAGEDIR_H
#define USERPROG_PAGEDIR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "vm/mmap.h"

uint32_t *pagedir_create (void);
void pagedir_destroy (uint32_t *pd);
bool pagedir_set_page (uint32_t *pd, void *upage, void *kpage, bool rw);
void *pagedir_get_page (uint32_t *pd, const void *upage);
void pagedir_clear_page (uint32_t *pd, void *upage);
bool pagedir_is_dirty (uint32_t *pd, const void *upage);
void pagedir_set_dirty (uint32_t *pd, const void *upage, bool dirty);
bool pagedir_is_accessed (uint32_t *pd, const void *upage);
void pagedir_set_accessed (uint32_t *pd, const void *upage, bool accessed);
void pagedir_activate (uint32_t *pd);
#ifdef VM
void pagedir_save_to_swap   (uint32_t *pd, const void *vpage);
bool pagedir_load_from_swap (uint32_t *pd, void *vpage);
bool pagedir_load_from_mmap (uint32_t *pd, void *vpage);
void pagedir_add_stack      (uint32_t *pd, void *vpage,
                             void *esp);
bool pagedir_setup_mmap     (uint32_t *pd, void *vpage,
                             mapid_t mapid, size_t size);
void pagedir_clear_mmap     (uint32_t *pd,  void *vpage,
                             size_t size);
#endif /* VM */

#endif /* userprog/pagedir.h */
