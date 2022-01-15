#include "vm/mmap.h"
#include <debug.h>
#include <list.h>
#include <round.h>
#include <stdbool.h>
#include <stddef.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/user-io.h"

struct mmap
{
  mapid_t          id;
  struct file     *file;
  uint8_t         *base;
  struct list_elem elem;
};



/* Internal function prototypes */
static mapid_t get_mapid (void);
static struct mmap *alloc_mmap (void);
static void free_mmap (struct mmap *mmap);
static struct mmap *find_mmap (mapid_t mid);


/* internal functions */
static mapid_t
get_mapid (void)
{
  return thread_current ()->next_fd++;
}

static struct mmap *
alloc_mmap (void)
{
  struct mmap *mmap = malloc (sizeof (struct mmap));

  if (mmap == NULL)
    return NULL;

  mmap->id = get_mapid ();

  return mmap;
}

static void
free_mmap (struct mmap *mmap)
{
  struct thread *t = thread_current ();
  size_t _fsize = file_length (mmap->file);

  size_t fsize = _fsize;
  for (uint8_t *page = mmap->base;
       (size_t) (page - mmap->base) < ROUND_UP (fsize, PGSIZE);
       page += PGSIZE, fsize -= PGSIZE)
    {
      if (pagedir_is_dirty (t->pagedir, page))
        {
          file_seek  (mmap->file, page - mmap->base);
          file_write (mmap->file, page, (fsize < PGSIZE) ? fsize : PGSIZE);
        }
    }
  pagedir_clear_mmap (t->pagedir, mmap->base, _fsize);
  file_close (mmap->file);
  list_remove (&mmap->elem);
  free (mmap);
}

static struct mmap *
find_mmap (mapid_t mid)
{
  struct thread    *t = thread_current ();
  struct list_elem *e;
  for (e = list_begin (&t->mmap);
       e != list_end (&t->mmap);
       e = list_next (e))
    {
      struct mmap *mmap = list_entry (e, struct mmap, elem);

      if (mmap->id == mid)
        return mmap;
    }

  return NULL;
}



/* External functions */

/* TODO */
mapid_t
mmap (struct file *file, void *addr)
{
  struct thread *t      = thread_current ();
  struct mmap   *mmap;
  size_t         fsize;

  if (addr == NULL)
    return -1; /* PintOS requirement */

  if ((fsize = file_length (file)) == 0)
    return -1;

  if ((mmap = alloc_mmap ()) == NULL)
    return -1;

  if (!pagedir_setup_mmap (t->pagedir, addr, mmap->id, fsize))
    {
      free (mmap);
      return -1;
    }

  mmap->base     = addr;
  mmap->file     = file_reopen (file);
  list_push_back (&t->mmap, &mmap->elem);

  return mmap->id;
}

/* TODO */
bool
mmap_load_page (mapid_t mid, void *page)
{
  struct mmap *mmap;

  if ((mmap = find_mmap(mid)) == NULL)
    return false;

  user_io_block ();
  file_seek (mmap->file, (uint8_t *)page - mmap->base);
  file_read (mmap->file, page, PGSIZE);
  user_io_release ();

  /* Dirty bits are used to check file write */
  pagedir_set_dirty (thread_current ()->pagedir, page, false);
  return true;
}

/* TODO */
void
munmap (mapid_t mid)
{
  struct mmap *mmap;

  if ((mmap = find_mmap(mid)) == NULL)
    return;

  free_mmap (mmap);
}

void
mmap_close_all (void)
{
  struct thread *t = thread_current ();

  struct list_elem *e = list_begin (&t->mmap);
  while (e != list_end (&t->mmap))
    {
      struct list_elem *next = list_next (e);
      struct mmap *mmap = list_entry (e, struct mmap, elem);

      user_io_block ();
      free_mmap (mmap);
      user_io_release ();
      e = next;
    }
}
