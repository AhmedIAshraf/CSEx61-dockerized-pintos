# Phase 1 Edition

## Priority Scheduler

### Thread struct changes

```c
struct thread
{
   int effictivePri;          /* Donated priority */
   struct lock *waitingOn;    /* Lock which the thread is currently waiting on */
   struct list locks;         /* List of locks that the thread holds */
};
```
### Lock struct changes

```c
struct lock
{
  int largestPri;           // highest priority of waiters
  struct list_elem elem;
};
```

### lock_acquire()
- Nested/Chain donation was added.
```c
void lock_acquire(struct lock *lock)
{
  ASSERT(lock != NULL);
  ASSERT(!intr_context());
  ASSERT(!lock_held_by_current_thread(lock));

  enum intr_level old_level;
  old_level = intr_disable();
  + thread_current()->waitingOn = lock;
  + struct lock *currentLock = lock;
  + struct thread *lockHolder = lock->holder;
  + int donation_level = 0;
  + while (lockHolder != NULL && thread_current()->effictivePri > lockHolder->effictivePri && !thread_mlfqs)
  + {
  +  lockHolder->effictivePri = thread_current()->effictivePri; // donate priority for lock holder
  +  currentLock->largestPri = lockHolder->effictivePri;
  +  currentLock = lockHolder->waitingOn;
  +  if (currentLock == NULL || donation_level == 8)
  +  {
  +    break;
  +  }
  +  lockHolder = currentLock->holder;
  +  donation_level++;
  + }
  sema_down(&lock->semaphore);
  + lock->holder = thread_current();
  + thread_current()->waitingOn = NULL;
  + if (!thread_mlfqs)
  + {
  +   lock->largestPri = thread_current()->effictivePri;
  +   list_insert_ordered(&thread_current()->locks, &lock->elem, locks_max_priority, NULL);
  + }
}
```

### lock_release()
- Multiple donation was added
```c
void lock_release(struct lock *lock)
{
  ASSERT(lock != NULL);
  ASSERT(lock_held_by_current_thread(lock));
  enum intr_level old_level;
  old_level = intr_disable();

  + lock->holder = NULL;

  + if (!thread_mlfqs)
  + {
  +   struct thread *th = thread_current();
  +   list_remove(&lock->elem);
  +   if (!list_empty(&th->locks))
  +   {
  +     struct lock *next_lock = list_entry(list_front(&th->locks), struct lock, elem);
  +     th->effictivePri = next_lock->largestPri;
  +   }
  +   else
  +   {
  +     th->effictivePri = th->priority;
  +   }
  + }
  sema_up(&lock->semaphore);
  intr_set_level(old_level);
}
```
### Also for priority scheduling we repalced all list_insert() with list_insert_ordered()