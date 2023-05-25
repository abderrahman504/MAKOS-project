#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"

static void syscall_handler (struct intr_frame *);
int get_int(int** esp);
char* get_char_ptr(char*** esp);
void* get_void_ptr(void* pt);


static struct lock files_sys_lock;

struct open_file* get_file(int fd);

void exit_wrapper(struct intr_frame *f);
void exec_wrapper(struct intr_frame *f);
void wait_wrapper(struct intr_frame *f);
void create_wrapper(struct intr_frame *f);
void remove_wrapper(struct intr_frame *f);
void open_wrapper(struct intr_frame *f);
void filesize_wrapper(struct intr_frame *f);
void read_wrapper(struct intr_frame *f);
void write_wrapper(struct intr_frame *f);
void seek_wrapper(struct intr_frame *f);
void tell_wrapper(struct intr_frame *f);
void close_wrapper(struct intr_frame *f);
void sys_exit(int status);
int sys_exec(char* file_name);
int sys_wait(int pid);
bool sys_create(char* name, size_t size);
bool sys_remove(char* name);
int sys_open(char* name);
int sys_filesize(int fd);
int sys_read(int fd,void* buffer, int size);
int sys_write(int fd, void* buffer, int size);
void sys_seek(int fd, unsigned pos);
unsigned sys_tell(int fd);
void sys_close(int fd);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&files_sys_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  validate_void_ptr(f->esp);

    switch (*(int*)f->esp) {
        case SYS_HALT:
            shutdown_power_off();
            break;

        case SYS_EXIT:
            exit_wrapper(f);
            break;

        case SYS_EXEC:
            exec_wrapper(f);
            break;

        case SYS_WAIT:
            wait_wrapper(f);
            break;

        case SYS_CREATE:
            create_wrapper(f);
            break;

        case SYS_REMOVE:
            remove_wrapper(f);
            break;

        case SYS_OPEN:
            open_wrapper(f);
            break;

        case SYS_FILESIZE:
            filesize_wrapper(f);
            break;

        case SYS_READ:
            read_wrapper(f);
            break;

        case SYS_WRITE:
            write_wrapper(f);
            break;

        case SYS_SEEK:
            seek_wrapper(f);
            break;

        case SYS_TELL:
            tell_wrapper(f);
            break;

        case SYS_CLOSE:
            close_wrapper(f);
            break;

        default:
            //panic
            break;
    }
}
void validate_void_ptr(const void* pt){
    if (pt == NULL || !is_user_vaddr(pt) || pagedir_get_page(thread_current()->pagedir, pt) == NULL)
    {
        sys_exit(-1);
    }
}

void exit_wrapper(struct intr_frame *f){
    validate_void_ptr(f->esp+4);
    int status = *((int*)f->esp + 1);
    sys_exit(status);
}
void sys_exit(int status){
    struct thread* parent = thread_current()->parent_thread;
    printf("%s: exit(%d)\n", thread_current()->name, status);
    if(parent) parent->child_status = status;
    thread_exit();
}

void exec_wrapper(struct intr_frame *f){
    validate_void_ptr(f->esp+4);
    char* name = (char*)(*((int*)f->esp + 1));

    if (name == NULL) sys_exit(-1);
    lock_acquire(&files_sys_lock);
    f->eax = sys_exec(name);
    lock_release(&files_sys_lock);
}
int sys_exec(char* file_name){
    return process_execute(file_name);
}

void wait_wrapper(struct intr_frame *f){
    validate_void_ptr(f->esp+4);
    int tid = *((int*)f->esp + 1);
    f->eax = sys_wait(tid);
}
int sys_wait(int pid){
    return process_wait(pid);
}

void create_wrapper(struct intr_frame *f){
    validate_void_ptr(f->esp + 4);
    validate_void_ptr(f->esp + 8);

    char* name = (char*)(*((int*)f->esp + 1));
    size_t size = *((int*)f->esp + 2);

    if (name == NULL) sys_exit(-1);

    f->eax = sys_create(name,size);
}
bool sys_create(char* name, size_t size){
    bool res;
    lock_acquire(&files_sys_lock);

    res = filesys_create(name,size);

    lock_release(&files_sys_lock);
    return res;
}

void remove_wrapper(struct intr_frame *f){
    validate_void_ptr(f->esp + 4);
    char* name = (char*)(*((int*)f->esp + 1));
    if (name == NULL) sys_exit(-1);
    f->eax = sys_remove(name);
}
bool sys_remove(char* name){
    bool res;
    lock_acquire(&files_sys_lock);
    res = filesys_remove(name);
    lock_release(&files_sys_lock);
    return res;
}

void open_wrapper(struct intr_frame *f){
    validate_void_ptr(f->esp + 4);
    char* name = (char*)(*((int*)f->esp + 1));
    if (name == NULL) sys_exit(-1);
    f->eax = sys_open(name);
}
int sys_open(char* name){
    struct open_file* open = palloc_get_page(0);
    if (open == NULL)
    {
        palloc_free_page(open);
        return -1;
    }
    lock_acquire(&files_sys_lock);
    open->ptr = filesys_open(name);
    lock_release(&files_sys_lock);
    if (open->ptr == NULL)
    {
        return -1;
    }
    open->fd = ++thread_current()->fd_last;
    list_push_back(&thread_current()->open_file_list,&open->elem);
    return open->fd;
}

void filesize_wrapper(struct intr_frame *f){
    validate_void_ptr(f->esp + 4);
    int fd = *((int*)f->esp + 1);

    f->eax = sys_filesize(fd);
}
int sys_filesize(int fd){
    struct thread* t = thread_current();
    struct file* my_file = get_file(fd)->ptr;

    if (my_file == NULL)
    {
        return -1;
    }
    int res;
    lock_acquire(&files_sys_lock);
    res = file_length(my_file);
    lock_release(&files_sys_lock);
    return res;
}

void read_wrapper(struct intr_frame *f)
{
    validate_void_ptr(f->esp + 4);
    validate_void_ptr(f->esp + 8);
    validate_void_ptr(f->esp + 12);

    int fd, size;
    void* buffer;
    fd = *((int*)f->esp + 1);
    buffer = (void*)(*((int*)f->esp + 2));
    size = *((int*)f->esp + 3);

    validate_void_ptr(buffer + size);

    f->eax = sys_read(fd,buffer,size);
}
int sys_read(int fd,void* buffer, int size){
    if (fd == 0)
    {

        for (size_t i = 0; i < size; i++)
        {
            lock_acquire(&files_sys_lock);
            ((char*)buffer)[i] = input_getc();
            lock_release(&files_sys_lock);
        }
        return size;

    } else {

        struct thread* t = thread_current();
        struct file* my_file = get_file(fd)->ptr;

        if (my_file == NULL)
        {
            return -1;
        }
        int res;
        lock_acquire(&files_sys_lock);
        res = file_read(my_file,buffer,size);
        lock_release(&files_sys_lock);
        return res;

    }
}

void write_wrapper(struct intr_frame *f)
{
    validate_void_ptr(f->esp + 4);
    validate_void_ptr(f->esp + 8);
    validate_void_ptr(f->esp + 12);

    int fd, size;
    void* buffer;
    fd = *((int*)f->esp + 1);
    buffer = (void*)(*((int*)f->esp + 2));
    size = *((int*)f->esp + 3);

    if (buffer == NULL) sys_exit(-1);

    f->eax = sys_write(fd,buffer,size);
}
int sys_write(int fd, void* buffer, int size)
{
    if (fd == 1)
    {
        lock_acquire(&files_sys_lock);
        putbuf(buffer,size);
        lock_release(&files_sys_lock);
        return size;

    } else {
        struct thread* t = thread_current();
        struct file* my_file = get_file(fd)->ptr;

        if (my_file == NULL)
        {
            return -1;
        }
        int res;
        lock_acquire(&files_sys_lock);
        res = file_write(my_file,buffer,size);
        lock_release(&files_sys_lock);
        return res;
    }
}
void seek_wrapper(struct intr_frame *f){
    validate_void_ptr(f->esp + 4);
    validate_void_ptr(f->esp + 8);

    int fd;
    unsigned pos;
    fd = *((int*)f->esp + 1);
    pos = *((unsigned*)f->esp + 2);

    sys_seek(fd,pos);
}
void sys_seek(int fd, unsigned pos){
    struct thread* t = thread_current();
    struct file* my_file = get_file(fd)->ptr;

    if (my_file == NULL)
    {
        return;
    }

    lock_acquire(&files_sys_lock);
    file_seek(my_file,pos);
    lock_release(&files_sys_lock);
}

void tell_wrapper(struct intr_frame *f)
{
    validate_void_ptr(f->esp + 4);
    int fd = *((int*)f->esp + 1);

    f->eax = sys_tell(fd);
}
unsigned sys_tell(int fd){
    struct thread* t = thread_current();
    struct file* my_file = get_file(fd)->ptr;

    if (my_file == NULL)
    {
        return -1;
    }

    unsigned res;
    lock_acquire(&files_sys_lock);
    res = file_tell(my_file);
    lock_release(&files_sys_lock);
    return res;
}

void close_wrapper(struct intr_frame *f){
    validate_void_ptr(f->esp + 4);
    int fd = *((int*)f->esp + 1);
    sys_close(fd);
}
void sys_close(int fd){
    struct thread* t = thread_current();
    struct open_file* my_file = get_file(fd);

    if (my_file == NULL)
    {
        return;
    }

    lock_acquire(&files_sys_lock);
    file_close(my_file->ptr);
    lock_release(&files_sys_lock);
    list_remove(&my_file->elem);
    palloc_free_page(my_file);
}


struct open_file* get_file(int fd){
    struct thread* t = thread_current();
    struct file* my_file = NULL;
    for (struct list_elem* e = list_begin (&t->open_file_list); e != list_end (&t->open_file_list);
         e = list_next (e))
    {
        struct open_file* opened_file = list_entry (e, struct open_file, elem);
        if (opened_file->fd == fd)
        {
            return opened_file;
        }
    }
    return NULL;
}

int get_int(int** esp)
{
	return *((int*)esp + 1);
}
char* get_char_ptr(char*** esp)
{

}
void* get_void_ptr(void* pt)
{

}