#include "userprog/syscall.h"
#include <console.h>
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

typedef int pid_t;

#define AW(T,PTR)              *((T *) safe_movl (PTR))
#define CALL_1(F,PTR,T1)       (F) (AW(T1,(PTR)+1))
#define CALL_2(F,PTR,T1,T2)    (F) (AW(T1,(PTR)+1), AW(T2,(PTR)+2))
#define CALL_3(F,PTR,T1,T2,T3) (F) (AW(T1,(PTR)+1), AW(T2,(PTR)+2), AW(T3,(PTR)+3))

static void syscall_handler (struct intr_frame *);

static bool  try_movl  (const uint32_t *src, uint32_t *dest);
static void *safe_movl (const uint32_t *src);

/* (START) system call wrappers prototype */
int  __write (int fd, const void *buffer, unsigned size);
void __exit  (int status);
/* (END  ) system call wrappers prototype */

static bool
try_movl (const uint32_t *src, uint32_t *dest)
{
  uint32_t result, temp;

  /* This code assumes that $1f is not on
     0xffffffff. It is impossilbe for an 32-bit
     architecture anyways. */

  asm ("movl $1f, %0; movl %1, %0; 1:"
       : "=&a" (result) : "m" (*src));
  if (result == 0xFFFFFFFF)
    return false;

  ASSERT (*src == result);
  temp = result;
  asm ("movl $1f, %0; movl %2, %1; 1:"
       : "=&a" (result), "=m" (*dest) : "r" (temp));
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

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  uint32_t **esp = &f->esp;
  uint32_t sys_num;

  sys_num = AW (uint32_t, *esp);

  switch (sys_num)
    {
    case SYS_HALT:
      shutdown_power_off ();
      break;
    case SYS_EXIT:
      CALL_1 (__exit, *esp, int);
      break;
    case SYS_EXEC:
    case SYS_WAIT:
    case SYS_CREATE:
    case SYS_REMOVE:
    case SYS_OPEN:
    case SYS_FILESIZE:
    case SYS_READ:
      PANIC ("%d : not implemented\n", *(uint32_t *)f->esp);
      thread_exit ();
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
      thread_exit ();
    }
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

void __exit (int status)
{
  struct thread *t = thread_current ();

  t->val = status;
  thread_exit();
}
/* (END  ) system call wrappers implementation */
