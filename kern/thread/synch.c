/*
 * Synchronization primitives.
 * See synch.h for specifications of the functions.
 */

#include <types.h>
#include <lib.h>
#include <synch.h>
#include <thread.h>
#include <curthread.h>
#include <machine/spl.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *namearg, int initial_count)
{
	struct semaphore *sem;

	assert(initial_count >= 0);

	sem = kmalloc(sizeof(struct semaphore));
	if (sem == NULL) {
		return NULL;
	}

	sem->name = kstrdup(namearg);
	if (sem->name == NULL) {
		kfree(sem);
		return NULL;
	}

	sem->count = initial_count;
	return sem;
}

void
sem_destroy(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);

	spl = splhigh();
	assert(thread_hassleepers(sem)==0);
	splx(spl);

	/*
	 * Note: while someone could theoretically start sleeping on
	 * the semaphore after the above test but before we free it,
	 * if they're going to do that, they can just as easily wait
	 * a bit and start sleeping on the semaphore after it's been
	 * freed. Consequently, there's not a whole lot of point in 
	 * including the kfrees in the splhigh block, so we don't.
	 */

	kfree(sem->name);
	kfree(sem);
}

void 
P(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);

	/*
	 * May not block in an interrupt handler.
	 *
	 * For robustness, always check, even if we can actually
	 * complete the P without blocking.
	 */
	assert(in_interrupt==0);

	spl = splhigh();
	while (sem->count==0) {
		thread_sleep(sem);
	}
	assert(sem->count>0);
	sem->count--;
	splx(spl);
}

void
V(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);
	spl = splhigh();
	sem->count++;
	assert(sem->count>0);
	thread_wakeup(sem);
	splx(spl);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
	struct lock *lock;

	lock = kmalloc(sizeof(struct lock));
	if (lock == NULL) {
		return NULL;
	}

	lock->name = kstrdup(name);
	if (lock->name == NULL) {
		kfree(lock);
		return NULL;
	}
	
	lock->lock_owner = NULL;

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	assert(lock != NULL);
	assert (lock->lock_owner == NULL); //check no thread holds the lock while the lock is being destroyed
	
	kfree(lock->name);
	kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
	assert (lock != NULL) //ensure that the lock exists

	//disable interrupts

	int spl = splhigh(); //spl captures the old state prior to disabling interrupts

	while (lock->lock_owner != NULL){

	thread_sleep(lock); //put any threads to sleep if they are not holding the lock 
	}

	assert (lock->lock_owner == NULL); //check that no thread is holding the lock

	//the thread can now acquire the lock

	lock->lock_owner = curthread; //assign the current thread to hold the lock

	//restore interrupts	

	splx(spl);
}

void
lock_release(struct lock *lock)
{
	assert (lock != NULL); //check the lock exists
	assert (lock_do_i_hold(lock)); //check the current thread holds the lock in the 1st place

	int spl = splhigh();

	lock->lock_owner = NULL;

	thread_wakeup(lock); //wakeup any thread(s) that were sleeping/waiting for that lock

	assert (lock->lock_owner == NULL);

	splx(spl);
}

int
lock_do_i_hold(struct lock *lock)
{

	return (lock->lock_owner == curthread);
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
	struct cv *cv;

	cv = kmalloc(sizeof(struct cv));
	if (cv == NULL) {
		return NULL;
	}

	cv->name = kstrdup(name);
	if (cv->name==NULL) {
		kfree(cv);
		return NULL;
	}
	
	cv->cv_lock = lock_create(name);

	return cv;
}

void
cv_destroy(struct cv *cv)
{
	assert(cv != NULL);
	lock_destroy(cv->cv_lock);

	kfree(cv->name);
	kfree(cv);
}

/* Release the supplied lock, go to sleep, and, after waking up again, re-acquire the lock.*/

void
cv_wait(struct cv *cv, struct lock *lock)
{
	assert((cv != NULL) && (lock != NULL));
	assert(lock_do_i_hold(lock));

	lock_acquire(cv->cv_lock);	

	lock_release(lock);

	lock_release(cv->cv_lock);

	int spl = splhigh();

	thread_sleep(cv);	
	
	splx(spl);

	//lock_acquire(cv->cv_lock);	

	lock_acquire(lock);

	//lock_release(cv->cv_lock);
}

/* Wake up one thread that's sleeping on this CV. */

void
cv_signal(struct cv *cv, struct lock *lock)
{
	assert((cv != NULL) && (lock != NULL));
	assert(lock_do_i_hold(lock));	



	lock_acquire(cv->cv_lock);	

	int spl = splhigh();

	thread_wakeone(cv);

	splx(spl);

	lock_release(cv->cv_lock);


}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	assert((cv != NULL) && (lock != NULL));
	assert(lock_do_i_hold(lock));	

	int spl = splhigh();

	lock_acquire(cv->cv_lock);	

	thread_wakeup(cv);	

	lock_release(cv->cv_lock);

	splx(spl);
}
