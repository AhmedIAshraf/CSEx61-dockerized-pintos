# Phase 2 Edition
## System Calls

### syscall_handler() in syscall.c
```c
static void
syscall_handler(struct intr_frame *f UNUSED)
{
    esp = f->esp;
    if (!is_user_vaddr((void *)(esp)))
    {
        exit(-1);
    }
    int system_type = *esp;
    switch (system_type)
    {
    case SYS_HALT:
        halt();
        break;

    case SYS_EXIT:
        if (!is_user_vaddr((void *)(esp + 1)))
        {
            exit(-1);
        }
        exit(*(esp + 1));
        break;

    case SYS_EXEC:
        if (!is_user_vaddr((void *)(esp + 1)))
        {
            exit(-1);
        }
        const char *file_name = (const char *)*(esp + 1);
        f->eax = exec(file_name);
        break;

    case SYS_WAIT:
        if (!is_user_vaddr((void *)(esp + 1)))
        {
            exit(-1);
        }
        f->eax = wait(*(esp + 1));
        break;

    case SYS_CREATE:
        if (!is_user_vaddr((void *)(esp + 1)) || !is_user_vaddr((void *)(esp + 2)))
        {
            exit(-1);
        }
        f->eax = create((char *)*(esp + 1), *(esp + 2));
        break;

    case SYS_REMOVE:
        if (!is_user_vaddr((void *)(esp + 1)))
        {
            exit(-1);
        }
        f->eax = remove((char *)*(esp + 1));
        break;

    case SYS_OPEN:
        if (!is_user_vaddr((void *)(esp + 1)))
        {
            exit(-1);
        }
        f->eax = open((char *)*(esp + 1));
        break;

    case SYS_FILESIZE:
        if (!is_user_vaddr((void *)(esp + 1)))
        {
            exit(-1);
        }
        f->eax = filesize(*(esp + 1));
        break;

    case SYS_READ:
        if (!is_user_vaddr((void *)(esp + 1)) || !is_user_vaddr((void *)(*(esp + 2))) || !is_user_vaddr((void *)(esp + 3)))
        {
            exit(-1);
        }
        f->eax = read(*(esp + 1), (void *)(*(esp + 2)), *(esp + 3));
        break;

    case SYS_WRITE:
        if (!is_user_vaddr((void *)(esp + 1)) || !is_user_vaddr((void *)(*(esp + 2))) || !is_user_vaddr((void *)(esp + 3)))
        {
            exit(-1);
        }
        f->eax = write(*(esp + 1), (void *)(*(esp + 2)), *(esp + 3));
        break;

    case SYS_SEEK:
        if (!is_user_vaddr((void *)(esp + 1)) || !is_user_vaddr((void *)(esp + 2)))
        {
            exit(-1);
        }
        seek(*(esp + 1), *(esp + 2));
        break;

    case SYS_TELL:
        if (!is_user_vaddr((void *)(esp + 1)))
        {
            exit(-1);
        }
        f->eax = tell(*(esp + 1));
        break;

    case SYS_CLOSE:
        if (!is_user_vaddr((void *)(esp + 1)))
        {
            exit(-1);
        }
        close(*(esp + 1));
        break;
    }
}
```
### Thread struct changes
```c
struct thread
{
    struct list open_files;
    int file_descriptor;
    // Parent - Children Communication
    struct thread *parent;              /* Thread/Process parent */
    struct list children;               /* List of thread/process children */
    int child_status;                  /* Child's -that the parent is waiting on- exit status */
    tid_t waiting_on_child_id;          /* Child's -that the parent is waiting on- id */
    struct semaphore wait_child_sema;   /* Semaphore which the parent waits on while waiting the child */
    struct list_elem child_elem;
    struct file *executable;            /* Loaded executable file */
};
```

### process_execute() in process.c
```c
tid_t process_execute(const char *file_name)
{
  char *fn_copy, *token, *save_ptr;
  tid_t tid;
  int argc = 0;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page(0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy(fn_copy, file_name, PGSIZE);

  char *new_file_name = malloc(strlen(file_name)+1);
  strlcpy(new_file_name,file_name,strlen(file_name)+1);
  new_file_name = strtok_r(new_file_name, " ", &save_ptr);
  tid = thread_create(new_file_name, PRI_DEFAULT, start_process, fn_copy);
  sema_down(&thread_current()->wait_child_sema);
  if (tid == TID_ERROR)
  {
    palloc_free_page(fn_copy);
  }
  return tid;
}
```

### start_process() in process.c
```c
static void
start_process(void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset(&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load(file_name, &if_.eip, &if_.esp);

  /* If load failed, quit. */
  palloc_free_page(file_name);
+  if (!success)
+  {
+    sema_up(&thread_current()->parent->wait_child_sema);
+    thread_exit();
+  }
+  else
+  {
+    sema_up(&thread_current()->parent->wait_child_sema);
+    sema_down(&thread_current()->wait_child_sema);
+  }
  asm volatile("movl %0, %%esp; jmp intr_exit" : : "g"(&if_) : "memory");
  NOT_REACHED();
}
```

### load() in syscall.c
```c
bool load(const char *file_name, void (**eip)(void), void **esp)
{
  *eip = (void (*)(void))ehdr.e_entry;
+  file_deny_write(file);
+  thread_current()->executable = file;
  success = true;

done:
-  file_close(file);
  return success;
}
```
### Wait in process.c
```c
+ bool is_child_valid(tid_t pid)
+ {
+   struct thread *cur = thread_current();
+   struct list_elem *c = list_begin(&cur->children);
+   for (; c != list_end(&cur->children); c = list_next(c))
+   {
+     struct thread *child = list_entry(c, struct thread, child_elem);
+     if (child->tid == pid)
+       return true;
+   }
+   return false;
+ }

+ struct thread *
+ get_child(tid_t pid)
+ {
+   struct thread *cur = thread_current();
+   struct list_elem *c = list_begin(&cur->children);
+   for (; c != list_end(&cur->children); c = list_next(c))
+   {
+     struct thread *child = list_entry(c, struct thread, child_elem);
+     if (child->tid == pid)
+       return child;
+   }
+ }

int process_wait(tid_t child_tid)
{
  + bool is_valid_child = is_child_valid(child_tid);
  + if (is_valid_child)
  + {
  +   struct thread *parent = thread_current();
  +   parent->waiting_on_child_id = child_tid;
  +   struct thread *child = get_child(child_tid);
  +   list_remove(&child->child_elem);
  +   sema_up(&child->wait_child_sema);
  +   sema_down(&parent->wait_child_sema);
  +   return parent->child_status;
  + }
  return -1;
}
```

### Exit in syscall.c
```c
void exit(int status)
{
    struct thread *cur = thread_current();
    struct list_elem *e = list_begin(&cur->open_files);
    printf("%s: exit(%d)\n", cur->name, status);
    /* Closing all files */
    for (; e != list_end(&cur->open_files); e = list_next(e))
    {
        struct open_file *f = list_entry(e, struct open_file, elem);
        close(f->fd);
    }

    struct thread *parent = cur->parent;
    if (parent->waiting_on_child_id == cur->tid)
    { // if thread is a child and there is a parent waiting on it
        parent->child_status = status;
        parent->waiting_on_child_id = -1;
        sema_up(&parent->wait_child_sema); // Wake up the parent
    }
    else
    { // else the parent not waiting on the child
        list_remove(&cur->child_elem);
    }
    /* Waking up all children */
    struct list_elem *c = list_begin(&cur->children);
    for (; c != list_end(&cur->children); c = list_next(c))
    {
        struct thread *child = list_entry(c, struct thread, child_elem);
        sema_up(&child->wait_child_sema); // Wake up the children
    }
    if(cur->executable){
        file_allow_write(cur->executable);
    }
    thread_exit();
}
```

### thread_create() in thread.c
```c
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
+  /* Set thread parent & add child */
+  struct thread *parent_thread = thread_current();
+  t->parent = parent_thread;
+  list_push_back(&parent_thread->children, &t->child_elem);
  /* Add to run queue. */
  thread_unblock (t);

  return tid;
}
```
### page_fault() in exception.c
```c
static void
page_fault(struct intr_frame *f)
{
    /* Determine cause. */
   not_present = (f->error_code & PF_P) == 0;
   write = (f->error_code & PF_W) != 0;
   user = (f->error_code & PF_U) != 0;

+   exit(-1);
}
```