#include "userprog/syscall.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "lib/kernel/console.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "lib/user/syscall.h"

static void syscall_handler(struct intr_frame *);
static struct lock file_sync_lock;
static uint32_t *esp;

void halt(void);
void exit(int);
pid_t exec (const char *);
bool create(const char *, unsigned);
bool remove(const char *);
int open(const char *);
int filesize(int);
int read(int, void *, unsigned);
int write(int,const void *, unsigned);
void seek(int, unsigned);
unsigned tell(int);
void close(int);
bool is_child_valid(tid_t);

void syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
    lock_init(&file_sync_lock);
}

static void
syscall_handler(struct intr_frame *f UNUSED) {
    printf("system call!\n");
    esp = f->esp;
    if (!is_user_vaddr((void *) (esp))) {
        exit(-1);
    }
    int system_type = *esp;
    switch (system_type) {
        case SYS_HALT:
            halt();
            break;

        case SYS_EXIT:
            if (!is_user_vaddr((void *) (esp + 1))) {
                exit(-1);
            }
            exit(*(esp + 1));
            break;

        case SYS_EXEC:
            if (!is_user_vaddr((void *) (esp + 1))) {
                exit(-1);
            }
            const char *file_name = (const char *) *(esp + 1);
            f->eax = exec(file_name);
            break;

        case SYS_WAIT: //implement
//            if (is_child_valid())
            break;

        case SYS_CREATE:
            if (!is_user_vaddr((void *) (esp + 1)) || !is_user_vaddr((void *) (esp + 2))) {
                exit(-1);
            }
            f->eax = create((char *) *(esp + 1), *(esp + 2));
            break;

        case SYS_REMOVE:
            if (!is_user_vaddr((void *) (esp + 1))) {
                exit(-1);
            }
            f->eax = remove((char *) *(esp + 1));
            break;

        case SYS_OPEN:
            if (!is_user_vaddr((void *) (esp + 1))) {
                exit(-1);
            }
            f->eax = open((char *) *(esp + 1));
            break;

        case SYS_FILESIZE:
            if (!is_user_vaddr((void *) (esp + 1))) {
                exit(-1);
            }
            f->eax = filesize(*(esp + 1));
            break;

        case SYS_READ:
            if (!is_user_vaddr((void *) (esp + 1)) || !is_user_vaddr((void *) (esp + 2)) || !is_user_vaddr((void *) (esp + 3))) {
                exit(-1);
            }
            f->eax = read(*(esp + 1), (void *) *(esp + 2), *(esp + 3));
            break;

        case SYS_WRITE:
            if (!is_user_vaddr((void *) (esp + 1)) || !is_user_vaddr((void *) (esp + 2)) || !is_user_vaddr((void *) (esp + 3))) {
                exit(-1);
            }
            f->eax = write(*(esp + 1), (void *) *(esp + 2), *(esp + 3));
            break;

        case SYS_SEEK:
            if (!is_user_vaddr((void *) (esp + 1)) || !is_user_vaddr((void *) (esp + 2))) {
                exit(-1);
            }
            seek(*(esp + 1), *(esp + 2));
            break;

        case SYS_TELL:
            if (!is_user_vaddr((void *) (esp + 1))) {
                exit(-1);
            }
            f->eax = tell(*(esp + 1));
            break;

        case SYS_CLOSE:
            if (!is_user_vaddr((void *) (esp + 1))) {
                exit(-1);
            }
            close(*(esp + 1));
            break;
    }
}

void
exit(int status) {
//    printf("exit");
    struct thread *cur = thread_current();
    struct list_elem *e = list_begin (&cur->open_files);
    printf("%s: exit(%d)\n", cur->name, status);

    for (; e != list_end (&cur->open_files); e = list_next (e))
    {
        struct open_file *f = list_entry (e, struct open_file, elem);
        close(f->fd);
    }

    struct thread *parent = cur->parent;
    if (parent->waiting_on_child_id == cur->tid) {
        parent->child_status = status;
        parent->waiting_on_child_id = -1;
        sema_up(&parent->wait_child_sema);
    } else {
        list_remove(&cur->child_elem);

        struct list_elem *c = list_begin (&cur->wait_child_sema.waiters);
        for (; c != list_end (&cur->wait_child_sema.waiters); c = list_next (c)) {
            struct thread *child = list_entry(c, struct thread, elem);
            sema_up(&cur->wait_child_sema);
        }
    }
    thread_exit();
}

void
halt () {
    shutdown_power_off();
}

pid_t exec (const char *file)
{
    pid_t t = process_execute(file);
    return t;
}

struct open_file *
get_file (int fd) {
    struct open_file *file;
    if (list_empty(&thread_current()->open_files)) exit(-1);
    struct list_elem *e = list_begin(&thread_current()->open_files);
    struct open_file *temp_file = list_entry(e, struct open_file, elem);
    if (temp_file->fd == fd) file = temp_file;
    else {
        while ((e = list_next(e)) != list_end(&thread_current()->open_files)) {
            temp_file = list_entry(e, struct open_file, elem);
            if (temp_file->fd == fd) {
                file = temp_file;
                break;
            }
        }
    }
    return file;
}

bool
create (const char *file, unsigned initial_size) {
    if (file == NULL) return false;
    lock_acquire(&file_sync_lock);
    bool success = filesys_create(file, initial_size);
    lock_release(&file_sync_lock);
    return success;
}

bool
remove (const char *file) {
    if (file == NULL) return false;
    lock_acquire(&file_sync_lock);
    bool success = filesys_remove(file);
    lock_release(&file_sync_lock);
    return success;
}

int
open (const char *file) {
    if (file == NULL) return -1;
    lock_acquire(&file_sync_lock);
    struct file *opened = filesys_open(file);
    int fd;
    if (opened != NULL) {
        fd = thread_current()->file_descriptor;
        thread_current()->file_descriptor++;
        struct open_file *temp;
        temp->fd = fd;
        temp->file = opened;
        list_push_back(&thread_current()->open_files, &temp->elem);
    } else
        fd = -1;
    lock_release(&file_sync_lock);
    return fd;
}

int
filesize (int fd) {
    lock_acquire(&file_sync_lock);
    struct open_file *file = get_file(fd);
    int size;
    if (file != NULL)
        size = file_length(file->file);
    lock_release(&file_sync_lock);
    if (file == NULL) return -1;
    return size;
}

int
read (int fd, void *buffer, unsigned size) {
    if (fd == 0) return input_getc();
    if (buffer == NULL) return -1;
    lock_acquire(&file_sync_lock);
    struct open_file *file = get_file(fd);
    int actual_size;
    if (file != NULL)
        actual_size = file_read(file->file, buffer, size);
    lock_release(&file_sync_lock);
    if (file == NULL) return -1;
    return actual_size;
}

int
write (int fd,const void *buffer, unsigned size) {
    if (buffer == NULL) return -1;
    if (fd == 1) {
        putbuf((char *) buffer, size);
        return size;
    }
    lock_acquire(&file_sync_lock);
    struct open_file *file = get_file(fd);
    int actual_size;
    if (file != NULL)
        actual_size = file_write(file->file, buffer, size);
    lock_release(&file_sync_lock);
    if (file == NULL) return -1;
    return actual_size;
}

void
seek (int fd, unsigned position) {
    lock_acquire(&file_sync_lock);
    struct open_file *file = get_file(fd);
    if (file != NULL) file_seek(file->file, position);
    lock_release(&file_sync_lock);
    return;
}

unsigned
tell (int fd) {
    lock_acquire(&file_sync_lock);
    struct open_file *file = get_file(fd);
    unsigned tell;
    if (file != NULL)
        tell = file_tell(file->file);
    lock_release(&file_sync_lock);
    if (file == NULL) return -1;
    return tell;
}

void
close (int fd) {
    lock_acquire(&file_sync_lock);
    struct open_file *file = get_file(fd);
    if (file != NULL) {
        file_close(file->file);
        list_remove(&file->elem);
    }
    lock_release(&file_sync_lock);
    return;
}

bool
is_child_valid(tid_t pid) {
    // TODO
    return true;
}
