/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <thread.h>
#include <curthread.h>
#include <vm.h>
#include <vfs.h>
#include <test.h>

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
static
int
align_target_str2 (char * str){
	return ((strlen(str)+4)/4);
}

int
runprogram(char *progname, char * args[], int argc)
{

	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, &v);
	if (result) {
		return result;
	}

	/* We should be a new thread. */
	assert(curthread->t_vmspace == NULL);

	/* Create a new address space. */
	curthread->t_vmspace = as_create();
	if (curthread->t_vmspace==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Activate it. */
	as_activate(curthread->t_vmspace);

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	// vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(curthread->t_vmspace, &stackptr);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
		return result;
	}

	//push user arguments to stack

		vaddr_t argv_ptrs_on_stack[argc];

		//we now push the required arguments onto the stack. SP will point to the
		//first argument, and so on. We will need to ensure that each pointer to
		//the argument is aligned.

		//push each argument(string) in reverse order. Make sure that the sp is aligned
		//by 4bytes. We should probably null-pad unused bytes, but leave it for now.

		int i;
		for (i = argc-1; i >= 0; i--){
			stackptr -= 4*align_target_str2(args[i]);
			copyoutstr(args[i], /*(userptr_t)*/stackptr, strlen(args[i])+1, NULL);
			argv_ptrs_on_stack[i] = stackptr;
		}

		//push the pointer to each argument now on the stack in reverse order.
		//Since each pointer is 4 bytes and the strings were aligned/padded we don't
		//check for alignment here.

		for(i = argc-1; i>=0; i--){
			stackptr -= 4;
			copyout(&(argv_ptrs_on_stack[i]), stackptr, sizeof(char *));
		}

	/* Warp to user mode. */
	md_usermode(argc, (userptr_t) stackptr,
		    stackptr, entrypoint);

	/* md_usermode does not return */
	panic("md_usermode returned\n");
	return EINVAL;
}
