#include "vm/swap-alloc.h"
#include <bitmap.h>
#include <debug.h>
#include <round.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "devices/block.h"
#include "threads/loader.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

/* TODO */

/* A swap page pool. */
struct pool
  {
    struct lock lock;                   /* Mutual exclusion. */
    struct bitmap *used_map;            /* Bitmap of free pages. */
  };

static struct block *swap_device;

static struct pool swap_pool;

static void init_pool (struct pool *, size_t page_cnt, const char *name);

#define SECTOR_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

/* Initializes the page allocator.  At most USER_PAGE_LIMIT
   pages are put into the user pool. */
void
swap_init (void)
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
  init_pool (&swap_pool, free_pages, "swap pool");
}

/* TODO */
size_t
swap_store_multiple (const void *_pages, size_t page_cnt)
{
  const int8_t  *pages = _pages;
  block_sector_t sector_idx;
  size_t         page_idx;

  ASSERT (pg_ofs (_pages) == 0);
  if (swap_device == NULL)
    return BITMAP_ERROR;
  if (page_cnt == 0)
    return BITMAP_ERROR;

  lock_acquire (&swap_pool.lock);
  page_idx = bitmap_scan_and_flip (swap_pool.used_map, 0, page_cnt, false);
  lock_release (&swap_pool.lock);

  if (page_idx != BITMAP_ERROR)
    sector_idx = page_idx * SECTOR_PER_PAGE;
  else
    PANIC ("swap_save: out of pages");

  for (size_t i=0; i < page_cnt * SECTOR_PER_PAGE; i++)
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
  int8_t *pages = _pages;
  size_t  sector_idx = page_idx * SECTOR_PER_PAGE;

  ASSERT (pg_ofs (_pages) == 0);
  if (swap_device == NULL)
    return;
  if (_pages == NULL || page_cnt == 0)
    return;

  for (size_t i=0; i < page_cnt * SECTOR_PER_PAGE; i++)
    block_read (swap_device, sector_idx + i, pages + (i * BLOCK_SECTOR_SIZE));

  ASSERT (bitmap_all (swap_pool.used_map, page_idx, page_cnt));
  bitmap_set_multiple (swap_pool.used_map, page_idx, page_cnt, false);
}

/* Frees the page at PAGE. */
void
swap_load_page      (size_t page_idx, void *page)
{
  swap_load_multiple (page_idx, page, 1);
}

bool
swap_is_valid (size_t page_idx)
{
  return page_idx < bitmap_size (swap_pool.used_map);
}

/* Initializes pool P as starting at START and ending at END,
   naming it NAME for debugging purposes. */
static void
init_pool (struct pool *p, size_t page_cnt, const char *name) 
{
  void *bbuf;

  size_t bm_pages = DIV_ROUND_UP (bitmap_buf_size (page_cnt), PGSIZE);
  bbuf = palloc_get_multiple (PAL_ZERO, bm_pages);

  if (bbuf == NULL)
    PANIC ("Not enough memory for %s bitmap.", name);

  printf ("%zu pages available in %s.\n", page_cnt, name);

  /* Initialize the pool. */
  lock_init (&p->lock);
  p->used_map = bitmap_create_in_buf (page_cnt, bbuf, bm_pages * PGSIZE);
}
