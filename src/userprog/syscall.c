#include "userprog/syscall.h"
#include <console.h>
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "threads/synch.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

typedef int pid_t;

static struct lock syscall_lock;

#define AW(T,PTR)              *((T *) safe_movl (PTR))
#define CALL_1(F,PTR,T1)       (F) (AW(T1,(PTR)+1))
#define CALL_2(F,PTR,T1,T2)    (F) (AW(T1,(PTR)+1), AW(T2,(PTR)+2))
#define CALL_3(F,PTR,T1,T2,T3) (F) (AW(T1,(PTR)+1), AW(T2,(PTR)+2), AW(T3,(PTR)+3))

static void syscall_handler (struct intr_frame *);

static bool  try_movl  (const uint32_t *src, uint32_t *dest);
static void *safe_movl (const uint32_t *src);
static void  assert_str_sanity (const char *str);

/* (START) system call wrappers prototype */


/* void halt (void) NO_RETURN; */
void __exit (int status) NO_RETURN;
int  __exec (const char *file);
int  __wait (pid_t);
/* bool create (const char *file, unsigned initial_size); */
/* bool remove (const char *file); */
/* int open (const char *file); */
/* int filesize (int fd); */
/* int read (int fd, void *buffer, unsigned length); */
int  __write (int fd, const void *buffer, unsigned size);
/* void seek (int fd, unsigned position); */
/* unsigned tell (int fd); */
/* void close (int fd); */
/* (END  ) system call wrappers prototype */

static bool
try_movl (const uint32_t *src, uint32_t *dest)
{
  uint32_t result;
  /* This code assumes that $1f is not on
     0xffffffff. It is impossilbe for an 32-bit
     architecture anyways. */

  asm ("movl $1f, %0; movl %1, %0"
       : "=&a" (result) : "m" (*src));
  asm ("movl %1, %0; 1:"
       : "=m" (*dest) : "a" (result));
  return (result != 0xFFFFFFFF);
}

static void *
safe_movl (const uint32_t *src)
{
  static uint32_t dest;
  if (!is_user_vaddr (src) || !try_movl(src, &dest))
    __exit (-1);
  return &dest;
}

/* Reads a byte at user address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}

/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}

static void
assert_str_sanity (const char *str)
{
  int c;
  int len = 0;

  while ((c = get_user (str)) != 0)
    {
      /* TODO: set reasonable limit for len */
      if (c == -1 || len > 255)
        __exit (-1);
      len++;
      str++;
    }

  if (!is_user_vaddr (str))
    __exit (-1);
}

void
syscall_init (void) 
{
  lock_init (&syscall_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  uint32_t **esp = &f->esp;
  uint32_t sys_num;

  sys_num = AW (uint32_t, *esp);

  lock_acquire (&syscall_lock);
  switch (sys_num)
    {
    case SYS_HALT:
      shutdown_power_off ();
      NOT_REACHED ();
      break;
    case SYS_EXIT:
      CALL_1 (__exit, *esp, int);
      NOT_REACHED ();
      break;
    case SYS_EXEC:
      CALL_1 (__exec, *esp, char *);
      break;
    case SYS_WAIT:
      CALL_1 (__wait, *esp, pid_t);
      break;
    case SYS_CREATE:
    case SYS_REMOVE:
    case SYS_OPEN:
    case SYS_FILESIZE:
    case SYS_READ:
      PANIC ("%d : not implemented\n", *(uint32_t *)f->esp);
      __exit (-1);
      break;
    case SYS_WRITE:
      f->eax = CALL_3 (__write, *esp, int, void *, unsigned);
      break;
    case SYS_SEEK:
    case SYS_TELL:
    case SYS_CLOSE:
      PANIC ("%d : not implemented\n", *(uint32_t *)f->esp);
      thread_exit ();
      break;
    default :
      __exit (-1);
    }
  lock_release (&syscall_lock);
}

/* (START) system call wrappers implementation */

int __write (int fd, const void *buffer, unsigned size)
{
  char *buf = buffer;

  if (fd != STDOUT_FILENO)
    {
      printf ("write() other than STDOUT not implemented (fd = %d)\n", fd);
      __exit (-1);
    }

  /* Sloppy implementation, I know. (TODO) */
  if (is_user_vaddr (buf + size -1)
      && get_user ((void *) buf) != -1
      && get_user ((void *) (buf + size -1)) != -1)
    putbuf (buf, size);
  else
    __exit (-1);

  return size;
}

void
__exit (int status)
{
  struct thread *t = thread_current ();

  /* Release syscall global lock since it will exit */
  if (lock_held_by_current_thread (&syscall_lock))
    lock_release (&syscall_lock);

  t->val = status;
  thread_exit();
}

int
__exec (const char *file)
{
  assert_str_sanity (file);

  return process_execute (file);
}

int
__wait (pid_t pid)
{
  return process_wait ((tid_t) pid);
}
/* (END  ) system call wrappers implementation */
