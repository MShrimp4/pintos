#include "userprog/user-io.h"
#include <stdio.h>
#include <console.h>
#include <debug.h>
#include <list.h>
#include "devices/input.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

/* typedefs */

struct user_file
{
  struct file *file;
  int fd;
  struct list_elem elem;
};


/* Global variables */

static struct lock io_lock;



/* Internal function prototypes */

static int  get_fd      (void);
static struct user_file *alloc_user_file (void);
static struct user_file *find_user_file (int fd);

static bool io_create   (const char *file, unsigned initial_size);
static bool io_remove   (const char *file);
static int  io_open     (const char *file);
static int  io_filesize (int fd);
static int  io_read     (int fd, void *buf, unsigned size);
static int  io_write    (int fd, const void *buf, unsigned size);
static void io_seek     (int fd, unsigned position);
static unsigned io_tell (int fd);
static void io_close    (int fd);

static void io_deny_write (int fd);


/* Internal functions */

static int
get_fd (void)
{
  return thread_current ()->next_fd++;
}

static struct user_file *
alloc_user_file ()
{
  struct user_file *ufile = malloc (sizeof (struct user_file));

  if (ufile == NULL)
    return NULL;

  ufile->fd = get_fd ();

  return ufile;
}

static struct user_file *
find_user_file (int fd)
{
  struct thread    *t = thread_current ();
  struct list_elem *e;
  for (e = list_begin (&t->file);
       e != list_end (&t->file);
       e = list_next (e))
    {
      struct user_file *ufile = list_entry (e, struct user_file, elem);

      if (ufile->fd == fd)
        return ufile;
    }

  return NULL;
}

static bool
io_create (const char *file, unsigned initial_size)
{
  return filesys_create (file, initial_size);
}

static bool
io_remove (const char *file)
{
  return filesys_remove (file);
}

static int
io_open (const char *file)
{
  struct user_file *ufile;

  if (file == NULL)
    return -1;

  if ((ufile = alloc_user_file ()) == NULL)
    return -1;

  if ((ufile->file = filesys_open (file)) == NULL)
    {
      free (ufile);
      return -1;
    }

  struct thread *t = thread_current ();

  list_push_back (&t->file, &ufile->elem);

  return ufile->fd;
}

int  io_filesize (int fd)
{
  struct user_file *ufile;

  if ((ufile = find_user_file (fd)) == NULL)
    return 0; /* Codes tend to assume filesize>=0 */

  return file_length (ufile->file);
}

int  io_read     (int fd, void *buf, unsigned size)
{
  struct user_file *ufile;


  if (fd == STDIN_FILENO)
    {
      uint8_t *buffer = buf;
      for (unsigned i=0; i<size; i++)
        buffer[i] = input_getc ();
      return size;
    }
  else if (fd == STDOUT_FILENO)
    return -1;

  if ((ufile = find_user_file (fd)) == NULL)
    return -1; /* Specified by PintOS */

  return file_read (ufile->file, buf, size);
}

int  io_write    (int fd, const void *buf, unsigned size)
{
  struct user_file *ufile;

  if (fd == STDIN_FILENO)
    return 0;
  else if (fd == STDOUT_FILENO)
    {
      putbuf (buf, size);
      return size;
    }

  if ((ufile = find_user_file (fd)) == NULL)
    return 0; /* Nothing is written */

  return file_write (ufile->file, buf, size);
}

void io_seek     (int fd, unsigned position)
{
  struct user_file *ufile;

  if ((ufile = find_user_file (fd)) == NULL)
    return;

  file_seek (ufile->file, position);
  return;
}

unsigned io_tell (int fd)
{
  struct user_file *ufile;

  if ((ufile = find_user_file (fd)) == NULL)
    return 0; /* Codes tend to assume index>=0 */

  return file_tell (ufile->file);
}

void io_close    (int fd)
{
  struct user_file *ufile;

  if ((ufile = find_user_file (fd)) == NULL)
    return;

  file_close (ufile->file);
  list_remove (&ufile->elem);
  free (ufile);
}

static void io_deny_write (int fd)
{
  struct user_file *ufile;

  if ((ufile = find_user_file (fd)) == NULL)
    return;

  file_deny_write (ufile->file);
}



/* External functions.
   Most functions are just lock handle + io_[some function](). */

void
user_io_init (void)
{
  lock_init (&io_lock);
}

void
user_io_block (void)
{
  lock_acquire (&io_lock);
}

void
user_io_release (void)
{
  lock_release (&io_lock);
}

void
user_io_close_all (void)
{
  struct thread *t    = thread_current ();
  struct list_elem *e = list_begin (&t->file);
  while (e != list_end (&t->file))
    {
      struct list_elem *next  = list_next (e);
      struct user_file *ufile = list_entry (e, struct user_file, elem);

      lock_acquire (&io_lock);
      file_close (ufile->file);
      lock_release (&io_lock);
      list_remove (&ufile->elem);
      free (ufile);

      e = next;
    }
}

bool
user_io_create (const char *file, unsigned initial_size)
{
  bool result;
  lock_acquire (&io_lock);
  result = io_create (file, initial_size);
  lock_release (&io_lock);
  return result;
}

bool
user_io_remove (const char *file)
{
  bool result;
  lock_acquire (&io_lock);
  result = io_remove (file);
  lock_release (&io_lock);
  return result;
}

int
user_io_open (const char *file)
{
  int fd;
  lock_acquire (&io_lock);
  fd = io_open (file);
  lock_release (&io_lock);
  return fd;
}

int
user_io_filesize (int fd)
{
  int size;
  lock_acquire (&io_lock);
  size = io_filesize (fd);
  lock_release (&io_lock);
  return size;
}

int
user_io_read (int fd, void *buf, unsigned size)
{
  int read_size;
  lock_acquire (&io_lock);
  read_size = io_read (fd, buf, size);
  lock_release (&io_lock);
  return read_size;
}

int
user_io_write (int fd, const void *buf, unsigned size)
{
  int write_size;
  lock_acquire (&io_lock);
  write_size = io_write (fd, buf, size);
  lock_release (&io_lock);
  return write_size;
}

void
user_io_seek (int fd, unsigned position)
{
  lock_acquire (&io_lock);
  io_seek (fd, position);
  lock_release (&io_lock);
}

unsigned
user_io_tell (int fd)
{
  unsigned position;
  lock_acquire (&io_lock);
  position = io_tell (fd);
  lock_release (&io_lock);
  return position;
}

void
user_io_close (int fd)
{
  lock_acquire (&io_lock);
  io_close (fd);
  lock_release (&io_lock);
}

void
user_io_deny_write (int fd)
{
  lock_acquire (&io_lock);
  io_deny_write (fd);
  lock_release (&io_lock);
}
