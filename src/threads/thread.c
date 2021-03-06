#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include <minmax.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "devices/timer.h"
#include "threads/vaddr.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list [PRI_MAX + 1];

/* List of processes sleeping.
   Semaphores have their own queues.*/
static struct list sleep_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Lock used to lock inter-thread edit */
static struct lock ital; /* Inter-Thread Action Lock */

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* TODO */
static ffloat load_avg;
static int    ready_threads;

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void decay_recent_cpu (struct thread *t, ffloat *decay_factor);
static void update_pri (struct thread *t, void *none UNUSED);
static bool update_recent_cpu (void);

static void set_pri (struct thread *t, int new_priority);
static int  get_pri (struct thread *t);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void thread_wakeup_sleepers (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);
static void thread_release_locks (void);




/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void)
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  lock_init (&ital);
  for (int i=PRI_MIN;i<=PRI_MAX;i++)
    list_init (&ready_list[i]);
  list_init (&all_list);
  list_init (&sleep_list);

  /* Set up a thread structure for the running thread. */
  ready_threads = 1;
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void)
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
  /* idle_thread is registered after the idle thread is
     unblocked, so thread_unblock wrongly counts up */
  ready_threads--;
}

static void
decay_recent_cpu (struct thread *t, ffloat *decay_factor)
{
  t->recent_cpu = f_add (f_mul (*decay_factor, t->recent_cpu), FFLOAT (t->nice));
}

static void
update_pri (struct thread *t, void *none UNUSED)
{
  ASSERT (is_thread (t));
  ASSERT (thread_mlfqs);

  if (t == idle_thread)
    {
      t->priority = 0;
      return;
    }

  int old_pri = t->priority;
  int new_pri = PRI_MAX - (F_TOINT (t->recent_cpu) / 4) - (t->nice * 2);
  new_pri = CLAMP (new_pri, PRI_MIN, PRI_MAX);

  t->priority = new_pri;
  if (old_pri != new_pri && t->status == THREAD_READY)
    {
      list_remove (&t->elem);
      list_push_back (&ready_list[new_pri], &t->elem);
    }
}

static bool
update_recent_cpu ()
{
  ffloat decay_factor;
  int64_t time = timer_ticks ();
  struct thread *t = thread_current ();

  t->recent_cpu = f_add (t->recent_cpu, FFLOAT (1));

  bool   update_priority = (time % 4 == 0);
  if (update_priority)
    thread_foreach ((thread_action_func *) update_pri, NULL);

  if (time % TIMER_FREQ == 0)
    {
      decay_factor = f_div (load_avg,
                            f_add (load_avg, f_div (FFLOAT (1), FFLOAT (2))));
        load_avg     = f_div
                         (f_add (f_mul (FFLOAT (59), load_avg),
                                 FFLOAT (ready_threads)),
                          FFLOAT (60));
      thread_foreach ((thread_action_func *) decay_recent_cpu, &decay_factor);
    }

  return update_priority;
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void)
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  if (thread_mlfqs)
    if (update_recent_cpu ())
      intr_yield_on_return ();

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void)
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux)
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();
#ifdef USERPROG
  {
    /* struct thread *t */
    struct thread       *cur  = thread_current ();
    struct return_value *rval = malloc (sizeof (struct return_value));
    if (rval == NULL)
      PANIC ("Failed to allocate return value storage");
    sema_init (&rval->sema, 0);
    rval->tid    = t->tid;
    rval->thread = t;
    rval->value  = t->val = 0;
    t->return_val = rval;
    list_push_back (&cur->child, &rval->elem);
  }
#endif /* USERPROG */

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  /* Add to run queue. */
  thread_unblock (t);

  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void)
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  if (thread_current () != idle_thread)
    ready_threads--;
  barrier ();
  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t)
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  ASSERT_CLAMP (t->priority, PRI_MIN, PRI_MAX);
  if (t != idle_thread)
    {
      list_push_back (&ready_list[get_pri (t)], &t->elem);
      ready_threads++;
    }
  t->status = THREAD_READY;
  intr_set_level (old_level);

  if (old_level == INTR_ON)
    thread_yield ();
}

/* Blocks the running thread until `time`. */
void
thread_wakemeupat (int64_t time)
{
  enum intr_level old_level;

  ASSERT (!intr_context ());

  old_level = intr_disable ();
  thread_current ()->wakeup_time = time;
  list_push_back (&sleep_list, &thread_current ()->elem);
  ASSERT (!list_empty(&sleep_list));
  thread_block ();
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void)
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void)
{
  struct thread *t = running_thread ();

  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void)
{
  return thread_current ()->tid;
}

/* Frees any holding locks. Semaphores are not managed by this
   function. */
static void
thread_release_locks ()
{
  struct thread *t = thread_current ();
  struct list_elem *e = list_begin (&t->locks);

  while (e != list_end (&t->locks))
    {
      struct list_elem *next = list_next (e);
      struct lock *lock = list_entry (e, struct lock, elem);
      ASSERT (lock_held_by_current_thread (lock));
      lock_release (lock);
      e = next;
    }
  /* clean lock list */
  list_init (&t->locks);
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void)
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  thread_release_locks ();

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  ready_threads--;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void)
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;

  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread)
    {
      ASSERT_CLAMP (cur->priority, PRI_MIN, PRI_MAX);
      list_push_back (&ready_list[get_pri (cur)], &cur->elem);
    }
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

static void
set_pri (struct thread *t, int new_priority)
{
  ASSERT (!thread_mlfqs);

  t->base_priority = new_priority;
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority)
{
  ASSERT (thread_current ()->status == THREAD_RUNNING);

  set_pri (thread_current (), new_priority);
  thread_yield ();
}

void
thread_donate_priority (struct thread *t, int donated_priority)
{
  ASSERT (!thread_mlfqs);

  t->priority = MAX (t->priority, donated_priority);
}

void
thread_update_donation (struct thread *t)
{
  if (thread_mlfqs) return;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (!thread_is_sleeping (t));
  ASSERT (!list_is_head (&t->elem));

  struct thread *holder;
  int priority = get_pri (t);
  struct list_elem *e = list_prev (&t->elem);

  /* If the thread is not a highest-priority thread
     then there is no need to update anything */
  if (!list_is_head (e))
    return;

  struct semaphore *sema = list_entry (e, struct semaphore, waiters.head);

  if (sema->holder == NULL)
    return;

  ASSERT (is_thread (holder = *sema->holder));

  thread_donate_priority (holder, priority);

  if (holder->status == THREAD_BLOCKED
      && !thread_is_sleeping (holder))
    {
      e = &holder->elem;
      struct list_elem *next_head = list_find_head(e);
      struct semaphore *next_sema
        = list_entry (next_head, struct semaphore, waiters.head);
      list_remove (e);
      list_insert_ordered (&next_sema->waiters, e,
                           (list_less_func *) sort_pri_descending, NULL);
      thread_update_donation (holder);
    }
}

void
thread_recover_donation ()
{
  if (thread_mlfqs) return;

  struct thread *t = thread_current ();
  ASSERT (intr_get_level () == INTR_OFF);

  /* Reset priority first */
  t->priority = PRI_MIN;

  /* e : element of locks = LOCK */
  /* l : LOCK */
  /* donor : element of waiters = THREAD */
  for (struct list_elem *e = list_begin (&t->locks);
       e != list_end (&t->locks);
       e = list_next (e))
    {
      struct lock *l = list_entry (e, struct lock, elem);
      if (list_empty (&l->semaphore.waiters))
        continue;
      struct thread *donor = list_entry (list_front (&l->semaphore.waiters),
                                         struct thread,
                                         elem);
      thread_donate_priority (t, get_pri (donor));
    }
}

static int
get_pri (struct thread *t)
{
  if (thread_mlfqs)
    return t->priority;
  else
    return MAX (t->priority, t->base_priority);
}

/* Returns the current thread's priority. */
int
thread_get_priority (void)
{
  return get_pri (thread_current ());
}

/* Sets the current thread's nice value to NICE and
   recalculate priority. */
void
thread_set_nice (int nice)
{
  int pri = thread_current ()->priority;

  ASSERT (thread_mlfqs);

  pri += 2 * (thread_current ()->nice - nice);
  thread_current ()->priority = CLAMP (pri, PRI_MIN, PRI_MAX);
  thread_current ()->nice = nice;
  thread_yield ();
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void)
{
  struct thread *t = thread_current ();

  ASSERT (thread_mlfqs);

  ASSERT_CLAMP (t->nice, -20, 20);
  return t->nice;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void)
{
  ASSERT (thread_mlfqs);

  return F_TOINT (f_round (f_mul (load_avg, FFLOAT (100))));
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void)
{
  ASSERT (thread_mlfqs);

  return F_TOINT (f_round (f_mul (thread_current ()->recent_cpu,
                                  FFLOAT (100))));
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED)
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;)
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux)
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void)
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT_CLAMP (priority, PRI_MIN, PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->base_priority = priority;
  t->priority      = PRI_MIN;
  t->wakeup_time   = INT64_MAX; /* Wake me up when September ends */
  list_init (&t->locks);
  if (running_thread () == t) /* initial thread */
    {
      t->nice = 0;
      t->recent_cpu = FFLOAT (0);
    }
  else
    {
      t->nice = running_thread ()->nice;
      t->recent_cpu = running_thread ()->recent_cpu;
    }
  t->magic = THREAD_MAGIC;
#ifdef USERPROG
  /* Local file descriptor management */
  list_init (&t->file);
  t->next_fd    = 3;
  /* Child management */
  list_init (&t->child);
  t->return_val = NULL;
  t->val        = 0;
#endif /* USERPROG */

#ifdef VM
  /* User ESP on pagefault while handling syscall */
  t->esp = NULL;
  /* Local memory map management */
  list_init (&t->mmap);
#endif /* VM */

  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size)
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Check if the thread is sleeping. Sleeping threads have
   its wakeup time set on wakeup_time. */
bool
thread_is_sleeping (struct thread *t)
{
  ASSERT (is_thread (t));
  return (t->wakeup_time != INT64_MAX);
}

/* Check wakeup time of sleepers and if the current time is
   later than that, unblock one.*/
static void
thread_wakeup_sleepers (void)
{
  struct list_elem *e = list_begin (&sleep_list);
  while (e != list_end (&sleep_list))
    {
      struct thread *t = list_entry (e, struct thread, elem);
      if (t->wakeup_time <= timer_ticks ())
        {
          t->wakeup_time = INT64_MAX;
          e = list_remove (e);
          thread_unblock (t);
        }
      else
        {
          e = list_next (e);
        }
    }
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void)
{
  ASSERT (intr_get_level () == INTR_OFF);

  thread_wakeup_sleepers ();

  for (int i=PRI_MAX;i>=PRI_MIN;i--)
    if (!list_empty (&ready_list[i]))
      return list_entry (list_pop_front (&ready_list[i]), struct thread, elem);

  return idle_thread;
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();

  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread)
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void)
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void)
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Lock other inter-thread action */
void
start_interthread_action ()
{
  lock_acquire (&ital);
}

/* Unlock inter-thread action lock */
void
end_interthread_action ()
{
  lock_release (&ital);
}

bool
sort_pri_descending (const struct list_elem *a,
                     const struct list_elem *b,
                     void *aux UNUSED)
{
  struct thread *a_t = list_entry (a, struct thread, elem);
  struct thread *b_t = list_entry (b, struct thread, elem);

  return thread_less_f (b_t, a_t);
}

bool
thread_less_f (struct thread *a, struct thread *b)
{
  return get_pri (a) < get_pri (b);
}


/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
