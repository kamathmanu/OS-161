#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#define KERNEL_ADDR	0x80000000
#define INVAL_ADDR	0x40000000
/*
 * Prototypes for IN-KERNEL entry points for system call implementations.
 */

/*********REBOOT*********/

int sys_reboot(int code);

/*********CONSOLE SYSCALLS*********/

int write (int fd, const void *buf, size_t size, int* retval);
int read (int fd, char *buf, size_t buflen, int* retval);

/*********PID MANAGEMENT SYSCALLS*********/

pid_t getpid();
void _exit(int code);
int waitpid(pid_t pid, int *returncode, int flags, int *retval);

/*********FORK*********/

struct fork_args
{
	struct trapframe *tf;
	struct addrspace *as;
	struct proc_info *new_proc;
};

int fork(struct trapframe *tf, int *retval);

// md_forkentry done right

void child_begins_here(void *args, unsigned long data2);

/*********EXECV*********/

int execv(const char *program, char *const *, int *retval);

/*********TIME*********/

time_t sys_time(time_t *seconds, unsigned long *nanoseconds, unsigned int *retval);

/********MALLOC********/

int sbrk(int amount, int * retval);


#endif /* _SYSCALL_H_ */
