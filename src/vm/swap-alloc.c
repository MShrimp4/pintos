#include "threads/palloc.h"
#include <bitmap.h>
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "threads/loader.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

/* Page allocator.  Hands out memory in page-size (or
   page-multiple) chunks.  See malloc.h for an allocator that
   hands out smaller chunks.

   System memory is divided into two "pools" called the kernel
   and user pools.  The user pool is for user (virtual) memory
   pages, the kernel pool for everything else.  The idea here is
   that the kernel needs to have memory for its own operations
   even if user processes are swapping like mad.

   By default, half of system RAM is given to the kernel pool and
   half to the user pool.  That should be huge overkill for the
   kernel pool, but that's just fine for demonstration purposes. */

/* A memory pool. */
struct pool
  {
    struct lock lock;                   /* Mutual exclusion. */
    struct bitmap *used_map;            /* Bitmap of free pages. */
    uint8_t *base;                      /* Base of pool. */
  };

static struct block *swap_device;

static struct pool swap_pool;

static void init_pool (struct pool *, void *base, size_t page_cnt,
                       const char *name);
static bool page_from_pool (const struct pool *, void *page);

#define SECTOR_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

/* Initializes the page allocator.  At most USER_PAGE_LIMIT
   pages are put into the user pool. */
void
swap_init ()
{
  ASSERT (PGSIZE % BLOCK_SECTOR_SIZE == 0);
  swap_device = block_get_role (BLOCK_SWAP);
  if (swap_device == NULL)
    {
      printf ("No swap device detected");
      return;
    }
  size_t free_pages = block_size (swap_device) / SECTOR_PER_PAGE;

  /* Give half of memory to kernel, half to user. */
  init_pool (&swap_pool, 0, free_pages, "swap pool");
}

/* TODO */
size_t
swap_store_multiple (const void *_pages, size_t page_cnt)
{
  int8_t        *pages = _pages;
  block_sector_t sector_idx;
  size_t         page_idx;

  if (page_cnt == 0)
    return NULL;

  lock_acquire (&pool->lock);
  page_idx = bitmap_scan_and_flip (pool->used_map, 0, page_cnt, false);
  lock_release (&pool->lock);

  if (page_idx != BITMAP_ERROR)
    sector_idx = page_idx * SECTOR_PER_PAGE;
  else
    PANIC ("swap_save: out of pages");

  for (size_t i=0; i < PAGE_CNT * SECTOR_PER_PAGE; i++)
    block_write (swap_device, sector_idx + i, pages + (i * BLOCK_SECTOR_SIZE));

  return page_idx;
}

/* TODO */
size_t
swap_store_page     (const void *page)
{
  return swap_store_multiple (page, 1);
}

/* TODO */
void
swap_load_multiple  (size_t page_idx, void *_pages, size_t page_cnt)
{
  size_t page_idx;

  ASSERT (pg_ofs (_pages) == 0);
  if (_pages == NULL || page_cnt == 0)
    return;

  for (size_t i=0; i < PAGE_CNT * SECTOR_PER_PAGE; i++)
    block_read (swap_device, sector_idx + i, pages + (i * BLOCK_SECTOR_SIZE));

  ASSERT (bitmap_all (swap_pool->used_map, page_idx, page_cnt));
  bitmap_set_multiple (swap_pool->used_map, page_idx, page_cnt, false);
}

/* Frees the page at PAGE. */
void
swap_load_page      (size_t page_idx, void *page)
{
  swap_load_multiple (page_idx, page, 1);
}

/* Initializes pool P as starting at START and ending at END,
   naming it NAME for debugging purposes. */
static void
init_pool (struct pool *p, void *base, size_t page_cnt, const char *name) 
{
  /* We'll put the pool's used_map at its base.
     Calculate the space needed for the bitmap
     and subtract it from the pool's size. */
  size_t bm_pages = DIV_ROUND_UP (bitmap_buf_size (page_cnt), PGSIZE);
  if (bm_pages > page_cnt)
    PANIC ("Not enough memory in %s for bitmap.", name);
  page_cnt -= bm_pages;

  printf ("%zu pages available in %s.\n", page_cnt, name);

  /* Initialize the pool. */
  lock_init (&p->lock);
  p->used_map = bitmap_create_in_buf (page_cnt, base, bm_pages * PGSIZE);
  p->base = base + bm_pages * PGSIZE;
}
