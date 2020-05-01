/*
 * catlock.c
 *
 * 30-1-2003 : GWA : Stub functions created for CS161 Asst1.
 *
 * NB: Please use LOCKS/CV'S to solve the cat syncronization problem in 
 * this file.
 */


/*
 * 
 * Includes
 *
 */

#include <types.h>
#include <lib.h>
#include <test.h>
#include <thread.h>
#include <synch.h>
#include <machine/spl.h>


/*
 * 
 * Constants
 *
 */

/*
 * Number of food bowls.
 */

#define NFOODBOWLS 2

/*
 * Number of cats.
 */

#define NCATS 6

/*
 * Number of mice.
 */

#define NMICE 2

//Number of meals

#define NMEALS 4

//Number to define animal eating from a bowl

#define NO_ONE 1000
#define CAT 1001
#define MOUSE 1002

//BOWL_NUMBERS

#define NO_BOWLS 0
#define BOWL_1 1
#define BOWL_2 2


//Definition of a "bowl" structure.

struct bowl{

	struct lock *lock;
	unsigned int number;
	unsigned int animal_eating; 
};

struct bowl *bowl_1, *bowl_2;
//struct lock* thread_access_counter_lock;
unsigned int thread_accesses = 0;

/*
 * 
 * Function Definitions
 * 
 */

//Function to initialize a bowl
struct bowl * 
bowl_init(unsigned int bowl_num) {

	struct bowl *bowl;

	bowl = kmalloc(sizeof(*bowl));
	if(bowl == NULL){
		return(NULL);
	}

	bowl->lock = lock_create("lock");	

	bowl->number = bowl_num;

	bowl->animal_eating = NO_ONE;

	return bowl;
}

//Function that returns a bowl that is not in use. If both bowls
// are empty, returns the 1st bowl. If both are use, returns an
// appropriate value.
unsigned int 
get_free_bowls(){

	if (bowl_1->lock->lock_owner == NULL){
		
		lock_acquire(bowl_1->lock);
		return (BOWL_1);
	}

	else if (bowl_2->lock->lock_owner == NULL){
		
		lock_acquire(bowl_2->lock);				
		return (BOWL_2);
	}

	else return (NO_BOWLS);
}

// Function to check if there's going to be a conflict between animals. A cat cannot be at either bowl
// if a mouse is eaiting from one of them, and vice versa.
unsigned int 
check_animal_status (struct bowl *bowl, unsigned int current_animal)
{
	if ((bowl->number == BOWL_1) && ((bowl_2->animal_eating == current_animal) || (bowl_2->animal_eating == NO_ONE))) {
		return 1;
	}		
	if ((bowl->number == BOWL_2) && ((bowl_1->animal_eating == current_animal) || (bowl_1->animal_eating == NO_ONE))) {
		return 1;
	}
	
	return 0;
}

void 
bowl_destroy(struct bowl * bowl){

	assert(bowl != NULL);
	lock_destroy(bowl->lock);
	kfree(bowl);
}

static void
wait_for_threads_to_finish()
{
	while(thread_accesses < (NCATS + NMICE)) {
      	thread_yield();
    }

    if (thread_accesses == (NMICE + NCATS)) {
		bowl_destroy(bowl_1);
		bowl_destroy(bowl_2);
		//lock_destroy(thread_access_counter_lock);
	}

}


/* who should be "cat" or "mouse" */
static void
lock_eat(const char *who, int num, int bowl, int iteration)
{
        kprintf("%s: %d starts eating: bowl %d, iteration %d\n", who, num, 
                bowl, iteration);
        clocksleep(1);
        kprintf("%s: %d ends eating: bowl %d, iteration %d\n", who, num, 
                bowl, iteration);
}

/*
 * catlock()
 *
 * Arguments:
 *      void * unusedpointer: currently unused.
 *      unsigned long catnumber: holds the cat identifier from 0 to NCATS -
 *      1.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Write and comment this function using locks/cv's.
 *
 */

static
void
catlock(void * unusedpointer, 
        unsigned long catnumber)
{
        (void) unusedpointer;

		unsigned int count_meals, free_bowl;

		count_meals = free_bowl = 0;

	//enter the while loop until each thread has eaten 4 times

		while (count_meals < NMEALS){
				
			free_bowl = get_free_bowls();

			if (free_bowl != NO_BOWLS) {

				if (free_bowl == BOWL_1 && check_animal_status(bowl_1, CAT)) {

					assert(((bowl_2->animal_eating == CAT) || (bowl_2->animal_eating == NO_ONE)));
					bowl_1->animal_eating = CAT;
					lock_eat("cat", catnumber, bowl_1->number,count_meals);
					count_meals++;
					lock_release(bowl_1->lock);
				 	bowl_1->animal_eating = NO_ONE;
					
				}
		
				else if (free_bowl == BOWL_2 && check_animal_status(bowl_2, CAT)) {

					assert(((bowl_1->animal_eating == CAT) || (bowl_1->animal_eating == NO_ONE)));					
					bowl_2->animal_eating = CAT;
					lock_eat("cat", catnumber, bowl_2->number, count_meals);					
					count_meals++;
					lock_release(bowl_2->lock);
				 	bowl_2->animal_eating = NO_ONE;

				}
				else {
					thread_yield();
				}
			}	
			else {
				thread_yield();
			}
			thread_yield();
		}

		//lock_acquire(thread_access_counter_lock);

		thread_accesses++;

		//lock_release(thread_access_counter_lock);
}
	

/*
 * mouselock()
 *
 * Arguments:
 *      void * unusedpointer: currently unused.
 *      unsigned long mousenumber: holds the mouse identifier from 0 to 
 *              NMICE - 1.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Write and comment this function using locks/cv's.
 *
 */

static
void
mouselock(void * unusedpointer,
          unsigned long mousenumber)
{
        (void) unusedpointer;
      
		unsigned int count_meals, free_bowl;

		count_meals = free_bowl = 0;

		while (count_meals < NMEALS) {
				
			free_bowl = get_free_bowls();

			if (free_bowl != NO_BOWLS) {

				if (free_bowl == BOWL_1 && check_animal_status(bowl_1, MOUSE)) {

					assert(((bowl_2->animal_eating == MOUSE) || (bowl_2->animal_eating == NO_ONE)));
					bowl_1->animal_eating = MOUSE;
					lock_eat("mouse", mousenumber, bowl_1->number,count_meals);
					count_meals++;
					lock_release(bowl_1->lock);
				 	bowl_1->animal_eating = NO_ONE;
				}
		
				else if (free_bowl == BOWL_2 && check_animal_status(bowl_2, MOUSE)) {

					assert(((bowl_1->animal_eating == MOUSE) || (bowl_1->animal_eating == NO_ONE)));
					bowl_2->animal_eating = MOUSE;
					lock_eat("mouse", mousenumber, bowl_2->number, count_meals);
					count_meals++;
					lock_release(bowl_2->lock);
				 	bowl_2->animal_eating = NO_ONE;

				}
				else {
					thread_yield();
				}
			}
			else {
				thread_yield();
			}
			thread_yield();
		}

		//lock_acquire(thread_access_counter_lock);

		thread_accesses++;

		//lock_release(thread_access_counter_lock);

}


/*
 * catmouselock()
 *
 * Arguments:
 *      int nargs: unused.
 *      char ** args: unused.
 *
 * Returns:
 *      0 on success.
 *
 * Notes:
 *      Driver code to start up catlock() and mouselock() threads.  Change
 *      this code as necessary for your solution.
 */
int
catmouselock(int nargs,
             char ** args)
{
        int index, error;
   
        /*
         * Avoid unused variable warnings.
         */

        (void) nargs;
        (void) args;
   

		bowl_1 = bowl_init(BOWL_1);
		bowl_2 = bowl_init(BOWL_2);
		//thread_access_counter_lock = lock_create("thread_access_counter_lock");

        /*
         * Start NCATS catlock() threads.
         */

		for (index = 0; index < NCATS; index++) {
				

                error = thread_fork("catlock thread", 
                                    NULL, 
                                    index, 
                                    catlock, 
                                    NULL
                                    );
                
                /*
                 * panic() on error.
                 */

                if (error) {
                 
                        panic("catlock: thread_fork failed: %s\n", 
                              strerror(error)
                              );
                }
        }


        /*
         * Start NMICE mouselock() threads.
         */

        for (index = 0; index < NMICE; index++) {
   				
                error = thread_fork("mouselock thread", 
                                    NULL, 
                                    index, 
                                    mouselock, 
                                    NULL
                                    );
      
                /*
                 * panic() on error.
                 */

                if (error) {
         
                        panic("mouselock: thread_fork failed: %s\n", 
                              strerror(error)
                              );
                }
        }

        wait_for_threads_to_finish();

        return 0;
}


/*
 * End of catlock.c
 */
