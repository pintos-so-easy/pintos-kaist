#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "lib/kernel/stdio.h"
#include "devices/input.h"
#include "threads/synch.h"
#include <string.h>

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
bool um_access (struct intr_frame *);
void check_address(void *addr);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

void check_address(void *addr) { struct thread *cur = thread_current(); if (addr == NULL || is_kernel_vaddr(addr) || pml4_get_page(cur->pml4, addr) == NULL) syscall_exit(-1);}

bool um_access (struct intr_frame *f) {
    return is_user_vaddr(f);
    }

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
    
    // if (um_access(f))
    
    switch (f->R.rax) {
    case SYS_HALT:
        syscall_halt();
        break;
    case SYS_EXIT:
        syscall_exit(f->R.rdi);
        break;
    case SYS_FORK:
        f->R.rax = syscall_fork(f->R.rdi);
        break;
    case SYS_EXEC:
        f->R.rax = syscall_exec(f->R.rdi);
        break;
    case SYS_WAIT:
        f->R.rax = syscall_wait();
        break;
    case SYS_CREATE:
        f->R.rax = syscall_create(f->R.rdi, f->R.rsi);
        break;
    case SYS_REMOVE:
        f->R.rax = syscall_remove(f->R.rdi);
        break;
    case SYS_OPEN:
        f->R.rax = syscall_open (f->R.rdi);
        break;
    case SYS_FILESIZE:
        f->R.rax = syscall_filesize (f->R.rdi);
        break;
    case SYS_READ:
        f->R.rax = syscall_read (f->R.rdi, f->R.rsi, f->R.rdx);
        break;
    case SYS_WRITE:
        // if (!um_access(f))
        //     syscall_exit(-1);
        f->R.rax = syscall_write(f->R.rdi, f->R.rsi, f->R.rdx);
        break;
    case SYS_SEEK:
        syscall_seek(f->R.rdi, f->R.rsi);
        break;
    case SYS_TELL:
        f->R.rax = syscall_tell(f->R.rdi);
        break;
    case SYS_CLOSE:
        syscall_close(f->R.rdi);
        break;
    default:
        syscall_exit(-1);
        break;
    }
	// printf ("system call!\n");
	// thread_exit ();
}

void syscall_halt(void)
{
	power_off();
}

void syscall_exit (int status){
    thread_current()->sys_status = status;
    thread_exit();
}
tid_t syscall_fork (const char *thread_name){

    memcpy(&thread_current()->pf,&thread_current()->tf,sizeof(struct intr_frame));
    tid_t pid = process_fork(thread_name,NULL);
    sema_down(&get_child_process(pid)->fork_sema);

	return pid;
}
int syscall_exec (const char *file){
    check_address(file);
    process_exec(file);

	return 0;
}
int syscall_wait (pid_t){
    tid_t pid = process_wait(pid_t);
	return pid;
}

bool syscall_create (const char *file, unsigned initial_size){
    // char lastChar = file[strlen(file)-1];
    check_address(file);
    if (file == NULL)
        syscall_exit(-1);
    // if (!um_access(file))
    //     syscall_exit(-1);
    // if (lastChar != '\0')
    //     syscall_exit(-1);
    // if (initial_size == 0)
    //     syscall_exit(-1);
    else
        return filesys_create(file, initial_size);
}
bool syscall_remove (const char *file){
    check_address(file);
    if (file == NULL)
        syscall_exit(-1);
	return filesys_remove(file);
}
int syscall_open (const char *file){
    check_address(file);
    if (file == NULL)
        syscall_exit(-1);
    struct file *open_file;
    // printf("%p\n",file);
    open_file = filesys_open(file);
    // printf("%p\n",open_file);
    if (open_file != NULL ) {
        for (int i = 3; i < 64; i++) {
            if (thread_current()->fd_table[i] == NULL) {
                thread_current()->fd_table[i] = open_file;
                return i;
            }   
        }
    }else
	    return -1;
}
int syscall_filesize (int fd){
    struct file *curr_file;
    curr_file = thread_current()->fd_table[fd];
    if (curr_file != NULL)
        return file_length(curr_file);
	else
        return -1;
}
int syscall_read (int fd, void *buffer, unsigned length){
    check_address(buffer);
    if (fd == 0){
        input_getc();
        return length;
    }else if (2<=fd && fd<64 && thread_current()->fd_table[fd] != NULL) {
        off_t read_byte = file_read(thread_current()->fd_table[fd],buffer,length);
        if (read_byte >= 0)
            return read_byte;
        else 
            return -1;
            // syscall_exit(-1);
    }else 
        syscall_exit(-1);
}

int syscall_write (int fd UNUSED, const void *buffer, unsigned size){

    check_address(buffer);
    if (fd == 1){
        putbuf(buffer,size);
        return size;
    }else if (2<=fd && fd<64 && thread_current()->fd_table[fd] != NULL)
    {
        off_t written_byte = file_write(thread_current()->fd_table[fd],buffer,size);
        return written_byte;
    }else
        syscall_exit(-1);
}

void syscall_seek (int fd, unsigned position){
    file_seek(thread_current()->fd_table[fd], position);

}
unsigned syscall_tell (int fd){
    off_t file_position;
    file_position = file_tell(thread_current()->fd_table[fd]);
	return file_position;
}
void syscall_close (int fd){
    if (fd == 0 || fd == 1)
        syscall_exit(-1);
    else if (2<=fd && fd<64 && thread_current()->fd_table[fd] != NULL) {
        file_close(thread_current()->fd_table[fd]);
        thread_current()->fd_table[fd] = NULL;
    }else
        syscall_exit(-1);
}
// int syscall_dup2(int oldfd, int newfd){

// 	return 0;
// }