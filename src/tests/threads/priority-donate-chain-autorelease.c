/* priority-donate-chain but without properly freeing last lock.
   Used to test auto-releasing locks when a thread is destroyed.

   Original version written by Godmar Back <gback@cs.vt.edu>
   Modified by Yongha Hwang <mshrimp@sogang.ac.kr> */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "threads/thread.h"

#define NESTING_DEPTH 8

struct lock_pair
{
  struct lock *second;
  struct lock *first;
};

struct fake_main_context
{
  struct lock main_lock;
  struct lock locks[NESTING_DEPTH - 1];
  struct lock_pair lock_pairs[NESTING_DEPTH];
};

static thread_func fake_main;
static thread_func donor_thread_func;
static thread_func interloper_thread_func;

void
test_priority_donate_chain_autorelease (void)
{
  char main_name[10] = "fake-main";
  struct fake_main_context ctx;

  /* This test does not work with the MLFQS. */
  ASSERT (!thread_mlfqs);

  thread_set_priority (PRI_MIN);

  lock_init (&ctx.main_lock);
  thread_create (main_name, PRI_DEFAULT, fake_main, &ctx);
  lock_acquire (&ctx.main_lock);

  msg ("%s finished.", main_name);
  lock_release (&ctx.main_lock);
}

static void
fake_main (void *aux)
{
  int i;
  struct fake_main_context *ctx = aux;

  lock_acquire (&ctx->main_lock);

  thread_set_priority (PRI_MIN);

  for (i = 0; i < NESTING_DEPTH - 1; i++)
    lock_init (&ctx->locks[i]);

  lock_acquire (&ctx->locks[0]);
  msg ("%s got lock.", thread_name ());

  for (i = 1; i < NESTING_DEPTH; i++)
    {
      char name[16];
      int thread_priority;

      snprintf (name, sizeof name, "thread %d", i);
      thread_priority = PRI_MIN + i * 3;
      ctx->lock_pairs[i].first = i < NESTING_DEPTH - 1 ? ctx->locks + i: NULL;
      ctx->lock_pairs[i].second = ctx->locks + i - 1;

      thread_create (name, thread_priority, donor_thread_func, ctx->lock_pairs + i);
      msg ("%s should have priority %d.  Actual priority: %d.",
          thread_name (), thread_priority, thread_get_priority ());

      snprintf (name, sizeof name, "interloper %d", i);
      thread_create (name, thread_priority - 1, interloper_thread_func, NULL);
    }
}

static void
donor_thread_func (void *locks_)
{
  struct lock_pair *locks = locks_;

  if (locks->first)
    lock_acquire (locks->first);

  lock_acquire (locks->second);
  msg ("%s got lock", thread_name ());

  lock_release (locks->second);
  msg ("%s should have priority %d. Actual priority: %d",
        thread_name (), (NESTING_DEPTH - 1) * 3,
        thread_get_priority ());
}

static void
interloper_thread_func (void *arg_ UNUSED)
{
  msg ("%s finished.", thread_name ());
}
