/*
 * Process system.
 */

#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <process.h>
#include <machine/spl.h>
#include <machine/pcb.h>
#include <thread.h>
#include <curthread.h>
#include <addrspace.h>
#include <vnode.h>
#include <synch.h>
#include <linked_list.h>
#include <array.h>

struct proc_info * process_table[MAX_PID];
unsigned int num_procs = 0;

//Function to assign a new PID to a process.
//We check for the smallest index in the PT
//that is free and assign the new process that
//PID.

pid_t assign_pid() {

	int i;
	for (i = 0; i < MAX_PID; i++) {

		if (process_table[i] == NULL) {

			num_procs++;
			return ((pid_t)(i + 1));
		}
	}

	/*PID can never be 0.*/
	return 0;
}

//Function to initialize the first process.

struct proc_info * process_bootstrap() {

	int i;

	for (i = 0; i < MAX_PID; i++) {
		process_table[i] = NULL;
	}

	struct proc_info *first = proc_create();

	if (first == NULL) {
		panic("No memory!");
	}

	num_procs = 1;
	first->parent_pid = 0;
	return first;

}

//Create a new process

struct proc_info *proc_create() {

	struct proc_info * proc = kmalloc(sizeof(*proc));

	if (proc == NULL) {
		return NULL;
	}

	proc->pid = assign_pid();
	proc->wait_sem = sem_create("wait_sem", 0);
	proc->exit_code = 0;

	process_table[proc->pid - 1] = proc;

	return proc;
}

//clean-up a process

void
proc_destroy(struct proc_info *proc) {

	assert(proc != NULL);

	num_procs--;
	process_table[proc->pid - 1] = NULL;
	sem_destroy(proc->wait_sem);

	kfree(proc);
}
