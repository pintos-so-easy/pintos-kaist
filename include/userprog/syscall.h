#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <stdbool.h>

typedef int tid_t;


void syscall_init (void);
void syscall_halt (void);
void syscall_exit (int status);
tid_t syscall_fork (const char *thread_name);
int syscall_exec (const char *file);
int syscall_wait (pid_t);
bool syscall_create (const char *file, unsigned initial_size);
bool syscall_remove (const char *file);
int syscall_open (const char *file);
int syscall_filesize (int fd);
int syscall_read (int fd, void *buffer, unsigned length);
int syscall_write (int fd, const void *buffer, unsigned length);
void syscall_seek (int fd, unsigned position);
unsigned syscall_tell (int fd);
void syscall_close (int fd);
int syscall_dup2(int oldfd, int newfd);

#endif /* userprog/syscall.h */
