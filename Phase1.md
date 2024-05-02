# Phase 1 Edition

## Priority Scheduler

### Thread struct changes

```c
struct thread
{
   int effictivePri;          // For Priority Donations
   struct lock *waitingOn;    // lock which the thread blocked by
   struct list locks;         // For locks the thread holds
   int nice;
   real recent_cpu;
   int64_t wakeup_ticks;  // time to wakeup
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
- Nested/Chain donation was edited.