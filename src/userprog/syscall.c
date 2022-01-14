#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "threads/synch.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/user-io.h"

typedef int pid_t;

#define AW(T,PTR)              *((T *) safe_movl (PTR))
#define CALL_1(F,PTR,T1)       (F) (AW(T1,(PTR)+1))
#define CALL_2(F,PTR,T1,T2)    (F) (AW(T1,(PTR)+1), AW(T2,(PTR)+2))
#define CALL_3(F,PTR,T1,T2,T3) (F) (AW(T1,(PTR)+1), AW(T2,(PTR)+2), AW(T3,(PTR)+3))

static void syscall_handler (struct intr_frame *);

/* (START) Memory sanity check functions */
static int   get_user (const uint8_t *uaddr);
static bool  put_user (uint8_t *udst, uint8_t byte) UNUSED;
static bool  try_movl  (const uint32_t *src, uint32_t *dest);
static void *safe_movl (const uint32_t *src);
static void  assert_arr_sanity (const uint8_t *arr, unsigned size);
static void  assert_str_sanity (const char *str);
/* (END  ) Memory sanity check functions */

/* (START) system call wrappers prototype */

/* void halt (void) NO_RETURN; */
static void __exit (int status) NO_RETURN;
static int  __exec (const char *file);
static int  __wait (pid_t);
static bool __create (const char *file, unsigned initial_size);
static bool __remove (const char *file);
static int  __open (const char *file);
static int  __filesize (int fd);
static int  __read (int fd, void *buffer, unsigned length);
static int  __write (int fd, const void *buffer, unsigned size);
static void __seek (int fd, unsigned position);
static unsigned __tell (int fd);
static void __close (int fd);

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
assert_arr_sanity (const uint8_t *arr, unsigned size)
{
  if (arr == NULL)
    __exit (-1);

  if (get_user ((void *)arr) == -1 || get_user ((void *)(arr +size-1)) == -1)
    __exit (-1);

  if (!is_user_vaddr (arr + size-1))
    __exit (-1);
}

static void
assert_str_sanity (const char *str)
{
  int c;

  if (str == NULL)
    __exit (-1);

  while ((c = get_user ((void *)str)) != 0)
    {
      if (c == -1 || !is_user_vaddr (str))
        __exit (-1);
      str++;
    }
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
#ifdef VM
  thread_current ()->esp = f->esp;
#endif /* VM */
  uint32_t **esp = (void *) (&f->esp);
  uint32_t sys_num;

  sys_num = AW (uint32_t, *esp);

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
      f->eax = CALL_1 (__exec, *esp, char *);
      break;
    case SYS_WAIT:
      f->eax = CALL_1 (__wait, *esp, pid_t);
      break;
    case SYS_CREATE:
      f->eax = CALL_2 (__create, *esp, char *, unsigned);
      break;
    case SYS_REMOVE:
      f->eax = CALL_1 (__remove, *esp, char *);
      break;
    case SYS_OPEN:
      f->eax = CALL_1 (__open, *esp, char *);
      break;
    case SYS_FILESIZE:
      f->eax = CALL_1 (__filesize, *esp, int);
      break;
    case SYS_READ:
      f->eax = CALL_3 (__read, *esp, int, void *, unsigned);
      break;
    case SYS_WRITE:
      f->eax = CALL_3 (__write, *esp, int, void *, unsigned);
      break;
    case SYS_SEEK:
      CALL_2 (__seek, *esp, int, unsigned);
      break;
    case SYS_TELL:
      f->eax = CALL_1 (__tell, *esp, int);
      break;
    case SYS_CLOSE:
      CALL_1 (__close, *esp, int);
      break;
    default :
      __exit (-1);
    }

#ifdef VM
  thread_current ()->esp = NULL;
#endif /* VM */
}

/* (START) system call wrappers implementation */

static void
__exit (int status)
{
  struct thread *t = thread_current ();

  t->val = status;
  thread_exit();
}

static int
__exec (const char *file)
{
  assert_str_sanity (file);

  return process_execute (file);
}

static int
__wait (pid_t pid)
{
  return process_wait ((tid_t) pid);
}


static bool
__create (const char *file, unsigned initial_size)
{
  assert_str_sanity (file);

  return user_io_create (file, initial_size);
}

static bool
__remove (const char *file)
{
  assert_str_sanity (file);

  return user_io_remove (file);
}

static int
__open (const char *file)
{
  assert_str_sanity (file);

  return user_io_open (file);
}


static int
__filesize (int fd)
{
  return user_io_filesize (fd);
}

static int
__read (int fd, void *buffer, unsigned length)
{
  assert_arr_sanity (buffer, length);

  return user_io_read (fd, buffer, length);
}

static int
__write (int fd, const void *buffer, unsigned size)
{
  assert_arr_sanity (buffer, size);

  return user_io_write (fd, buffer, size);
}

static void
__seek (int fd, unsigned position)
{
  return user_io_seek (fd, position);
}

static unsigned
__tell (int fd)
{
  return user_io_tell (fd);
}

static void
__close (int fd)
{
  return user_io_close (fd);
}

/* (END  ) system call wrappers implementation */
