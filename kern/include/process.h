#ifndef _PROCESS_H_
#define _PROCESS_H_

//The process abstraction. Note: this is actually NOT exactly the same as the usual
//process abstraction for any UNIX/BSD-like OS; the reason being since OS/161 processes
//are single-threaded at all times, we take advantage of this any define process info
//within the thread structure.

#define MAX_PID 500

struct proc_info {
	pid_t pid;
	struct semaphore * wait_sem;
	int exit_code;
	// int exited;
	pid_t parent_pid;
};

typedef struct proc_info proc_info;

extern struct thread * menu_thread;
extern struct lock * menu_thread_lock;
extern struct cv * menu_thread_cv;

extern struct proc_info * process_table[MAX_PID];
extern unsigned int num_procs;

// Allocate a new pid for the process we are currently trying to create
// (called in proc_create).
pid_t assign_pid();

// Initializes the very first process created, called in cmd_progthread.
struct proc_info * process_bootstrap();

// Creates a new 'process'.
struct proc_info *proc_create();

// Destroys the given 'process'.
void proc_destroy(struct proc_info *proc);

#endif /* _PROCESS_H_ */
