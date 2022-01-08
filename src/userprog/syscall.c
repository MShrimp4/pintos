#include "userprog/syscall.h"
#include <console.h>
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "threads/interrupt.h"
#include "threads/thread.h"

typedef int pid_t;

#define aw(t,ptr)              *((t *) (ptr))
#define call_1(f,ptr,t1)       (f) (aw(t1,ptr+1))
#define call_2(f,ptr,t1,t2)    (f) (aw(t1,ptr+1), aw(t2,ptr+2))
#define call_3(f,ptr,t1,t2,t3) (f) (aw(t1,ptr+1), aw(t2,ptr+2), aw(t3,ptr+3))

static void syscall_handler (struct intr_frame *);

/* (START) system call wrappers prototype */
int  __write (int fd, const void *buffer, unsigned size);
void __exit  (int status);
/* (END  ) system call wrappers prototype */

static bool
try_movl (const uint32_t *src, uint32_t *dest)
{
  int result;
  /* This code assumes that $1f is not on
     0xffffffff. It is impossilbe for an 32-bit
     architecture anyways. */

  asm ("movl $1f, %0; movl %2, %1; 1:"
       : "=&a" (result) , "=m" (*dest) : "m" (*src));
  return result != -1;
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
  int32_t *esp = (int32_t *)f->esp;

  switch (*esp)
    {
    case SYS_HALT:
      shutdown_power_off ();
      break;
    case SYS_EXIT:
      call_1 (__exit, esp, int);
      break;
    case SYS_EXEC:
    case SYS_WAIT:
    case SYS_CREATE:
    case SYS_REMOVE:
    case SYS_OPEN:
    case SYS_FILESIZE:
    case SYS_READ:
      PANIC ("%d : not implemented\n", *esp);
      thread_exit ();
      break;
    case SYS_WRITE:
      f->eax = call_3 (__write, esp, int, void *, unsigned);
      break;
    case SYS_SEEK:
    case SYS_TELL:
    case SYS_CLOSE:
      PANIC ("%d : not implemented\n", *esp);
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
  if (get_user (buffer) != -1
      && get_user (buffer + size -1) != -1)
    putbuf (buffer, size);
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
