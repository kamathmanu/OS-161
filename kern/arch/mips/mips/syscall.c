#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <machine/pcb.h>
#include <machine/spl.h>
#include <machine/trapframe.h>
#include <kern/callno.h>
#include <kern/unistd.h>
#include <kern/limits.h>
#include <syscall.h>
#include <curthread.h>
#include <thread.h>
#include <process.h>
#include <addrspace.h>
#include <synch.h>
#include <array.h>
#include <vfs.h>
#include <vnode.h>
#include <vm.h>
#include <clock.h>

#define MAX_PATH_SIZE 128
#define MAX_STRING 128 //assume same as path length
#define MAXMENUARGS 16 //defined in menu.c


//NOTE: REMOVE ALL THE DEBUG PRINTF STATEMENTS!

/*
 * System call handler.
 *
 * A pointer to the trapframe created during exception entry (in
 * exception.S) is passed in.
 *
 * The calling conventions for syscalls are as follows: Like ordinary
 * function calls, the first 4 32-bit arguments are passed in the 4
 * argument registers a0-a3. In addition, the system call number is
 * passed in the v0 register.
 *
 * On successful return, the return value is passed back in the v0
 * register, like an ordinary function call, and the a3 register is
 * also set to 0 to indicate success.
 *
 * On an error return, the error code is passed back in the v0
 * register, and the a3 register is set to 1 to indicate failure.
 * (Userlevel code takes care of storing the error code in errno and
 * returning the value -1 from the actual userlevel syscall function.
 * See src/lib/libc/syscalls.S and related files.)
 *
 * Upon syscall return the program counter stored in the trapframe
 * must be incremented by one instruction; otherwise the exception
 * return code will restart the "syscall" instruction and the system
 * call will repeat forever.
 *
 * Since none of the OS/161 system calls have more than 4 arguments,
 * there should be no need to fetch additional arguments from the
 * user-level stack.
 *
 * Watch out: if you make system calls that have 64-bit quantities as
 * arguments, they will get passed in pairs of registers, and not
 * necessarily in the way you expect. We recommend you don't do it.
 * (In fact, we recommend you don't use 64-bit quantities at all. See
 * arch/mips/include/types.h.)
 */

void
mips_syscall(struct trapframe *tf)
{
	int callno;
	int32_t retval;
	int err;

	assert(curspl==0);

	callno = tf->tf_v0;

	/*
	 * Initialize retval to 0. Many of the system calls don't
	 * really return a value, just 0 for success and -1 on
	 * error. Since retval is the value returned on success,
	 * initialize it to 0 by default; thus it's not necessary to
	 * deal with it except for calls that return other values,
	 * like write.
	 */

	retval = 0;

	switch (callno) {
	    case SYS_reboot:
		err = sys_reboot(tf->tf_a0);
		break;

		case SYS_fork:
		err = fork(tf, &retval);
		break;

	    case SYS_write:
	    err = write(tf->tf_a0, (char*)tf->tf_a1, tf->tf_a2, &retval);
	    break;

	    case SYS_read:
	    err = read(tf->tf_a0, (char *) tf->tf_a1, tf->tf_a2, &retval);
	    break;

	    case SYS_getpid:
	    err = 0; //never fails
	    retval = getpid();
	    break;

	    case SYS__exit:
	    _exit(tf->tf_a0);
	    break;

	    case SYS_waitpid:
	    err = waitpid(tf->tf_a0, (int *)tf->tf_a1, tf->tf_a2, &retval);
	    break;

	    case SYS_execv:
	    err = execv((char*)tf->tf_a0, (char **)tf->tf_a1, &retval);
			break;

			case SYS___time:
			err = sys_time((time_t *)tf->tf_a0, (unsigned long *)tf->tf_a1, &retval);
			break;

			case SYS_sbrk:
			err = sbrk(tf->tf_a0, &retval);
			break;

	    default:
			kprintf("Unknown syscall %d\n", callno);
			err = ENOSYS;
			break;
	}


	if (err) {
		/*
		 * Return the error code. This gets converted at
		 * userlevel to a return value of -1 and the error
		 * code in errno.
		 */
		tf->tf_v0 = err;
		tf->tf_a3 = 1;      /* signal an error */
	}
	else {
		/* Success. */
		tf->tf_v0 = retval;
		tf->tf_a3 = 0;      /* signal no error */
	}

	/*
	 * Now, advance the program counter, to avoid restarting
	 * the syscall over and over again.
	 */

	tf->tf_epc += 4;

	/* Make sure the syscall code didn't forget to lower spl */
	assert(curspl==0);
}

//the function the forked child starts execution in.
//Here we copy the trapframe and then begin the
//switch to user mode. We also free the copied trapframe
//as that is our responsibility.

void
child_begins_here(void *args, unsigned long data2) {

	(void)data2;

	struct fork_args *copied_args = (struct fork_args*) args;
	struct trapframe child_tf = *(copied_args->tf);

	//curthread->t_proc = copied_args->new_proc;
	//struct proc_info *new_proc = proc_create();

	// curthread->t_proc->p_pid = (pid_t)data2; //set the parent PID for the child

	curthread->t_vmspace = copied_args->as;
	as_activate(curthread->t_vmspace); //for lab 3 this becomes useful

	kfree(copied_args->tf);
	kfree(copied_args);

	//Set the success values.

	struct trapframe *newtf = &child_tf;

	newtf->tf_v0 = 0;
	newtf->tf_a3 = 0;

	//Increment PC so that we go to next instruction

	newtf->tf_epc += 4;

	//return to user mode.

	mips_usermode(&child_tf);
	panic("Returned from user mode, this is not normal");
}

int
fork(struct trapframe *tf, int *retval){

	if (num_procs == MAX_PID){
		*retval = -1;
		return (EAGAIN); //too many processes already exist.
	}

	struct thread *newthread;

	struct proc_info *new_proc = NULL;

	int spl = splhigh();

	//copy the address space of the parent.
	struct addrspace *as_child = kmalloc(sizeof(*as_child)); //AS_CREATE!!!!
	if (as_child == NULL){
		*retval = -1;
		splx(spl);
		return ENOMEM;
	}

	int copy_vmspace = as_copy(curthread->t_vmspace, &as_child);
	if(copy_vmspace != 0){
		kfree(as_child);
		*retval = -1;
		splx(spl);
		return ENOMEM;
	}
//	as_activate(curthread->t_vmspace); //Use for VM lab

	//Before the child runs after getting forked,
	//there might be a trap back to user level, thereby
	//changing the current trapframe. We preserve the tf
	//by saving it onto the heap, which the child is responsible
	//for freeing.

	struct trapframe *tf_copy = kmalloc(sizeof(*tf_copy));
	if (tf_copy == NULL){
			*retval = -1;
			splx(spl);
			return (ENOMEM);
	}
	memcpy((struct trapframe*)tf_copy, (struct trapframe*)tf, sizeof(struct trapframe));

	struct fork_args *args = kmalloc(sizeof(*args));

	args->tf = tf_copy;
	args->as = as_child;
	args->new_proc = new_proc;

	//now call thread_fork

	int fork_result;

	// struct proc_info * pro = curthread->t_proc;
	// struct thread * cur = curthread;

	fork_result = thread_fork(curthread->t_name, (void *)args,
		(unsigned long) curthread->t_proc->pid,
		child_begins_here, &newthread);

	if(fork_result){
		kfree(tf_copy);
		kfree(as_child);
		kprintf("Fork is failing");
		*retval = -1;
		splx(spl);
		return (*retval);
	}

	//fork succeeded

	newthread->t_proc = proc_create();

	if (newthread->t_proc == NULL) {
		*retval = -1;
		splx(spl);
		return ENOMEM;
	}

	newthread->t_proc->parent_pid = curthread->t_proc->pid;

	//Return the child's PID from the parent's end

	*retval = newthread->t_proc->pid;

	//enable interrupts

	splx(spl);

	return (0);
}

/******CONSOLE SYSCALLS: write(), read()********/

int
write (int fd, const void *buf, size_t size, int *retval){

	//This function simply uses the kernel printf to write
	//to the file descriptor.

	char * buf_c = (char*) buf;
	int err_handle;

	size_t n_bytes_written = 0;
	char* write_buf = kmalloc(size);

	if (buf == NULL || buf == (char *)0xbadbeef || buf == (char *)0xdeadbeef)
		return EFAULT;

	int spl = splhigh(); //buf is a shared resource and we are modifying it

	if (fd == STDOUT_FILENO || fd == STDERR_FILENO){

	err_handle = copyin((const_userptr_t)buf, write_buf, size);
	if (err_handle){
		splx(spl);
		kfree(write_buf);
		return (EFAULT);
	}

	while (n_bytes_written < size){

		putch(*buf_c);
		buf_c++;
		n_bytes_written++; //increment number of bytes written
	}

	*retval = n_bytes_written;

	splx(spl);
	kfree(write_buf);
	return (0);
	}

	else{
		kfree(write_buf);
		splx(spl);
		return EBADF;
	}
}

int
read (int fd, char *buf, size_t buflen, int * retval){

	// size_t num_bytes_read = 0;

	if (fd != STDIN_FILENO){
		return (EBADF);
	}

	if (buf == NULL || buf == (char *)0xbadbeef || buf == (char *)0xdeadbeef)
		return (EFAULT);

	// int spl = splhigh();

	char *read = kmalloc(sizeof(char));

	*read = getch();

	int err_handle = copyout(read, (userptr_t) buf, buflen);
	if (err_handle){
		kfree(read);
		return (EFAULT);
	}

	kfree(read);

	// splx(spl);

	*retval = 1;

	return(0);
}

/******PID FUNCTIONS: getpid(), wait(), _exit()********/

pid_t
getpid() {
	return curthread->t_proc->pid;
}

void
_exit(int code) {

	//Synchronizes the menu thread.
	if (curthread == menu_thread) {

		lock_acquire(menu_thread_lock);
		cv_broadcast(menu_thread_cv, menu_thread_lock);
		lock_release(menu_thread_lock);

	}

	//Collect the exit code passed in. (Saved in process member variable.)
	curthread->t_proc->exit_code = code;

	//Wake up parent that is sleeping on this child.
	V(curthread->t_proc->wait_sem);

	thread_exit();
}

int waitpid (pid_t pid, int * returncode, int flags, int * retval) {

	*retval = pid;
	struct proc_info * child_process = process_table[pid - 1];

	if (
		(flags != 0) 									  //Should always be 0.
		||
		(curthread->t_proc->pid == pid) //Makes no sense for the current thread to have itself as a child.
		||
		(pid < MIN_PID) 								//Range check.
		||
		(pid > MAX_PID)
		||
		(child_process == NULL)			    //Process doesn't exist.
		||
		(!process_table[pid - 1]->parent_pid == curthread->t_proc->pid)
		//Above condition checks that the parent of the child process passed in is
		//actually curthread.
	)
	{
		return EINVAL;
	}

	//Check the validity of the returncode (status) of the child passed in.
	if (
			(returncode == NULL)
			||
			(returncode == (int *)(INVAL_ADDR))
			||
			(returncode == (int *)(KERNEL_ADDR))
		)
	{
		return EFAULT;
	}

	//Copyin process for variable from userland.
	int * status = kmalloc(sizeof(int));
	int result = copyin((const_userptr_t)returncode, status, sizeof(int));

	if (result) {
		return EFAULT;
	}

	//Now, the parent process will sleep on this child process until it wakes it
	//up in _exit(), after submitting its exit code.
	P(child_process->wait_sem);

	///PARENT HAS BEEN WOKEN UP BY CHILD IN _exit()

	//Grab the exit code the child has submitted, and update the status ptr with
	//that information.
	*status = child_process->exit_code;

	//Destroy the child process that has now exited.
	proc_destroy(child_process);

	//Copy out the now updated status ptr with the exit code the child submitted
	//in _exit() to userland. Return 0 on a successful copyout.
	return (copyout(status, (userptr_t) returncode, sizeof(int)) != 0) ? EFAULT : 0;

}

/***EXECV**************/

// static
// uint32_t
// align_data((void*));


// //push the arguments to the user stack.

// static
// void
// push_args_to_stack(int nargs, vaddr_t stackptr){

// }

//Helper function that reads in arguments from userspace ptr **args,
//which is the argument vector - a NULL terminated array of
// null terminating string arguments. We return the number of
// args,i.e. argc in the argument vector.

static
char **
copy_argv (char *const *args, int *err_flag, int *argc){

	int nargs = 0; //we want
	char temp_string[MAX_STRING];
	 /* count the number of args. */

	while (args[nargs] != NULL){
		nargs++;
	}

	char ** k_args = kmalloc((nargs+1)*sizeof(userptr_t));
	if (k_args == NULL){
		*err_flag = ENOMEM;
		return NULL;
	}

	//check args points to valid region

	size_t * len = kmalloc(sizeof(size_t));

	*err_flag = copycheck((userptr_t)args, (size_t)(nargs+1), len);

	if (*err_flag != 0) {
		return NULL;
	}

	int i;
	for (i = 0; i < nargs; i++){
		//copy strings to temporary, and check for validity
		//of each userspace arg

		*err_flag = copyinstr((const_userptr_t)args[i],temp_string,MAX_STRING,NULL);

		if (*err_flag != 0) {
			return NULL;
		}

		//allocate the argument to the kernel buffer

		k_args[i] = kmalloc(strlen(temp_string)+1); //+1 for '\0'

		if (k_args[i] == NULL) {
			*err_flag = EFAULT;
			return NULL;
		}

		strcpy(k_args[i], temp_string);
	}

	//append the null pointer to the kernel buffer

	k_args[nargs] = NULL;

	//copy nargs to return value argc

	//kfree(len);

	*argc = nargs;

	return k_args; //success

}

//returns how much the stack_ptr needs to be shifted by to be aligned

static
int
align_target_str (char * str){
	return ((strlen(str)+4)/4);
}

//main execv syscall.

int
execv(const char *program, char *const *argv, int *retval) {

	int argc; //number of arguments in the argument vector
	int result; //err checking flag
	char k_progname[MAX_PATH_SIZE]; //temp buffer for path name
	char **k_args_buf; 	//kernel buffer where we will transfer our userspace arguments

	if (
			(program == NULL)
			||
			(argv == NULL)
			||
			(program == (int *)(INVAL_ADDR))
			||
			(argv == (int *)(INVAL_ADDR))
			||
			(program == (int *)(KERNEL_ADDR))
			||
			(argv == (int *)(KERNEL_ADDR))
		)
	{
		return EFAULT;
	}


	//disable intterupts for atomicity. Again, using a lock
	//is probably better but I cbf.

	int spl = splhigh();

	//check validity of path name address. Just for safety.

	result = copyinstr((const_userptr_t) program, k_progname, (size_t)MAX_PATH_SIZE, NULL);

	if (result) {
		if (result == ENAMETOOLONG)
			result = E2BIG;
			kfree(result);
		splx(spl);
		return (result);
	}

	if (strlen(k_progname) == 0) {
		splx(spl);
		return EINVAL;
	}

	//copy the arguments from argv to the kernel buffer

	k_args_buf = copy_argv(argv, &result, &argc);

	if (result) {
		splx(spl);
		return(result);
	}

	if (k_args_buf == NULL){
		splx(spl);
		return(ENOMEM);
	}

	//this is quite similar structure to runprogram

	struct vnode *v;
	struct addrspace *as_old, *as_new; //not sure what the as_old will be
	vaddr_t entrypoint, stackptr;

	as_old = curthread->t_vmspace; //the address space prior to switching to the new process

	/* Open the file. */
		result = vfs_open(k_progname, O_RDONLY, &v);
		if (result) {
			splx(spl);
			return result;
		}

	/* We should have an address space. */
	assert(curthread->t_vmspace != NULL);

	/* Create a new address space and activate it. */
	curthread->t_vmspace = as_create();
	if (curthread->t_vmspace == NULL){
		vfs_close(v);
		splx(spl);
		return ENOMEM;
	}
	as_activate(curthread->t_vmspace);
	as_new = curthread->t_vmspace;

	/* Load the executable. */
		result = load_elf(v, &entrypoint);
		if (result) {
			/* thread_exit destroys curthread->t_vmspace */
			as_destroy(as_new);
			vfs_close(v);
			//*retval = -1;
			splx(spl);
			return result;
		}

	/* Define the user stack in the address space */
		result = as_define_stack(as_new, &stackptr);
		if (result) {
			/* thread_exit destroys curthread->t_vmspace */
			as_destroy(as_new);
			vfs_close(v);
			//*retval = -1;
			splx(spl);
			return result;
		}

	/* Done with the file now. */
		// vfs_close(v);

	//push user arguments to stack

		vaddr_t argv_ptrs_on_stack[argc+1];

		//we now push the required arguments onto the stack. SP will point to the
		//first argument, and so on. We will need to ensure that each pointer to
		//the argument is aligned.

		//push each argument(string) in reverse order. Make sure that the sp is aligned
		//by 4bytes. We should probably null-pad unused bytes, but leave it for now.

		int i;
		for (i = argc-1; i >= 0; i--){
			stackptr -= 4*align_target_str(k_args_buf[i]);
			copyoutstr(k_args_buf[i], stackptr, strlen(k_args_buf[i])+1, NULL);
			argv_ptrs_on_stack[i] = stackptr;
		}

		//push the pointer to each argument now on the stack in reverse order.
		//Since each pointer is 4 bytes and the strings were aligned/padded we don't
		//check for alignment here. Also k_args_buf[argc] = NULL so we take care of that.

		for(i = argc; i>=0; i--){
			stackptr -= 4;
			copyout(&(argv_ptrs_on_stack[i]), stackptr, sizeof(char *));
		}

		//destroy old address-space here because we are sure execv will succeed so
		//can make destructive changes.

		//kfree(k_args_buf);
		as_destroy(as_old);

		//re-enable interrupts

		splx(spl);

	/* Warp to user mode. */
		md_usermode(argc, (userptr_t) stackptr,
			    stackptr, entrypoint);

		/* md_usermode does not return */
		panic("md_usermode returned\n");
		return EINVAL;
}

/********************TIME******************************/

time_t sys_time(time_t *seconds, unsigned long *nanoseconds, unsigned int *retval) {

	//Kfree when returning from error?

	int result;
	time_t *ksec = kmalloc(sizeof(time_t));
	unsigned long *knan = kmalloc(sizeof(unsigned long));

	gettime(ksec,(u_int32_t *)knan);

	if (seconds != NULL){
		result = copyout(ksec,(userptr_t)seconds,sizeof(time_t));
		if (result){
			return EFAULT;
		}
	}

	if (nanoseconds != NULL){
		result = copyout(knan,(userptr_t)nanoseconds, sizeof(unsigned long));
		if (result){
			return EFAULT;
		}
	}

	*retval = *ksec;

	kfree(ksec);
	kfree(knan);

	return (0);
}

/****************************SBRK********************************/

int sbrk(int amount, int *retval) {

	// amount = amount & PAGE_FRAME; //must be page aligned

	struct addrspace* as = curthread->t_vmspace;

	if (amount % 4 != 0){
		ROUNDUP(amount, 4); //check if its word aligned
	}

	if ((as->as_brk + amount) < (as->memory_segments[HEAP].v_base)){
		//trying to touch the data segment/go below the initial heap value
		return EINVAL;
	}

	if (as->as_brk + amount >= as->heap_max){
		//not enough heap memory! We're flowing into the stack
		return ENOMEM;
	}

	//use the actual segment info for debugging purposes. In reality this code is not
	//to pleasing, as we're still probably giving enough info for an attacker
	//to exploit the heap info. But this ain't the security lab, so...

	*retval = (as->as_brk);
	as->as_brk += amount;
	return 0;
}
