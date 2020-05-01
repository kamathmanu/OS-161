/* 
 * stoplight.c
 *
 * 31-1-2003 : GWA : Stub functions created for CS161 Asst1.
 *
 * NB: You can use any synchronization primitives available to solve
 * the stoplight problem in this file.
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
#include <queue.h>
#include <curthread.h>
#include <machine/spl.h>

/*
 *
 * Constants
 *
 */

/*
 * Number of cars created.
 */

#define NCARS 20
#define NREGIONS 4

/*Global variables.*/

struct semaphore *intersection_sem, *car_counter_sem;

enum { NW = 0, NE = 1, SE = 2, SW = 3};

struct semaphore *quadrant[NREGIONS]; 
struct semaphore *quadrant_queue_sem[NREGIONS]; 
struct semaphore *quadrant_gate_sem[NREGIONS]; 
struct queue *quadrant_queue[NREGIONS]; 

volatile unsigned int cars_directed;

/*End of global variables.*/

/*
 *
 * Function Definitions
 *
 */

static const char *directions[] = { "N", "E", "S", "W" };

static const char *turn_directions[] = { "straight", "left", "right"};

static const char *msgs[] = {
        "approaching:",
        "region1:    ",
        "region2:    ",
        "region3:    ",
        "leaving:    "
};

/* use these constants for the first parameter of message */
enum { APPROACHING, REGION1, REGION2, REGION3, LEAVING };


static void
message(int msg_nr, int carnumber, int cardirection, int destdirection)
{
        kprintf("%s car = %2d, direction = %s, destination = %s\n",
                msgs[msg_nr], carnumber,
                directions[cardirection], directions[destdirection]);
}

static
int
find_destination_direction(int cardirection, int turn)
{
	int destination_direction = -1;

    if (directions[cardirection] == "N") {
       	
       	if (turn_directions[turn] == "straight") {
       		destination_direction = 2;
       	}
       	else if (turn_directions[turn] == "left") {
       		destination_direction = 1;
       	}
       	else if (turn_directions[turn] == "right") {
       		destination_direction = 3;
       	}
    }
    else if (directions[cardirection] == "S") {
       	
       	if (turn_directions[turn] == "straight") {
       		destination_direction = 0;
       	}
       	else if (turn_directions[turn] == "left") {
       		destination_direction = 3;
       	}
       	else if (turn_directions[turn] == "right") {
       		destination_direction = 1;
       	}
    }
    else if (directions[cardirection] == "E") {
       	
       	if (turn_directions[turn] == "straight") {
       		destination_direction = 3;
       	}
       	else if (turn_directions[turn] == "left") {
       		destination_direction = 2;
       	}
       	else if (turn_directions[turn] == "right") {
       		destination_direction = 0;
       	}
    }
    else if (directions[cardirection] == "W") {
       	
       	if (turn_directions[turn] == "straight") {
       		destination_direction = 1;
       	}
       	else if (turn_directions[turn] == "left") {
       		destination_direction = 0;
       	}
       	else if (turn_directions[turn] == "right") {
       		destination_direction = 2;
       	}
    }

    return destination_direction;
}

 
/*
 * gostraight()
 *
 * Arguments:
 *      unsigned long cardirection: the direction from which the car
 *              approaches the intersection.
 *      unsigned long carnumber: the car id number for printing purposes.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      This function should implement passing straight through the
 *      intersection from any direction.
 *      Write and comment this function.
 */

static
void
gostraight(unsigned long cardirection,
           unsigned long carnumber)
{
		
		P(intersection_sem);

        if (directions[cardirection] == "N") {

        	/*Approaching from: north
        	  Going: south*/

        	P(quadrant[NW]);
        	
        	message(REGION1, carnumber, cardirection, 2);

        	V(quadrant_gate_sem[NW]);

        	P(quadrant[SW]);

        	message(REGION2, carnumber, cardirection, 2);

       		V(quadrant[NW]);

        	message(LEAVING, carnumber, cardirection, 2);

        	V(quadrant[SW]);

        }
        else if (directions[cardirection] == "S") {

        	/*Approaching from: south
        	  Going: north*/

        	P(quadrant[SE]);
        	
        	message(REGION1, carnumber, cardirection, 0);

        	V(quadrant_gate_sem[SE]);

        	P(quadrant[NE]);

        	message(REGION2, carnumber, cardirection, 0);

       		V(quadrant[SE]);

        	message(LEAVING, carnumber, cardirection, 0);

        	V(quadrant[NE]);
        }
        else if (directions[cardirection] == "E") {		
        	
        	/*Approaching from: east
        	  Going: west*/

        	P(quadrant[NE]);
        	
        	message(REGION1, carnumber, cardirection, 3);

        	V(quadrant_gate_sem[NE]);

        	P(quadrant[NW]);

        	message(REGION2, carnumber, cardirection, 3);

       		V(quadrant[NE]);

        	message(LEAVING, carnumber, cardirection, 3);

        	V(quadrant[NW]);
        }
        else if (directions[cardirection] == "W") {

        	/*Approaching from: west
        	  Going: east*/

        	P(quadrant[SW]);
        	
        	message(REGION1, carnumber, cardirection, 1);

        	V(quadrant_gate_sem[SW]);

        	P(quadrant[SE]);

        	message(REGION2, carnumber, cardirection, 1);

       		V(quadrant[SW]);

        	message(LEAVING, carnumber, cardirection, 1);

        	V(quadrant[SE]);
        }

		V(intersection_sem);
}


/*
 * turnleft()
 *
 * Arguments:
 *      unsigned long cardirection: the direction from which the car
 *              approaches the intersection.
 *      unsigned long carnumber: the car id number for printing purposes.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      This function should implement making a left turn through the 
 *      intersection from any direction.
 *      Write and comment this function.
 */

static
void
turnleft(unsigned long cardirection,
         unsigned long carnumber)
{
P(intersection_sem);

        if (directions[cardirection] == "N") {

        	/*Approaching from: north
        	  Going: east*/

        	P(quadrant[NW]);
        	
        	message(REGION1, carnumber, cardirection, 1);

        	V(quadrant_gate_sem[NW]);

        	P(quadrant[SW]);

        	message(REGION2, carnumber, cardirection, 1);

       		V(quadrant[NW]);

        	P(quadrant[SE]);

        	message(REGION3, carnumber, cardirection, 1);

        	V(quadrant[SW]);

        	message(LEAVING, carnumber, cardirection, 1);

        	V(quadrant[SE]);

        }
        else if (directions[cardirection] == "S") {

        	/*Approaching from: south
        	  Going: west*/

        	P(quadrant[SE]);
        	
        	message(REGION1, carnumber, cardirection, 3);

        	V(quadrant_gate_sem[SE]);

        	P(quadrant[NE]);

        	message(REGION2, carnumber, cardirection, 3);

       		V(quadrant[SE]);

        	P(quadrant[NW]);

        	message(REGION3, carnumber, cardirection, 3);

        	V(quadrant[NE]);

        	message(LEAVING, carnumber, cardirection, 3);

        	V(quadrant[NW]);
        }
        else if (directions[cardirection] == "E") {		
        	
        	/*Approaching from: east
        	  Going: south*/

        	P(quadrant[NE]);
        	
        	message(REGION1, carnumber, cardirection, 2);

        	V(quadrant_gate_sem[NE]);

        	P(quadrant[NW]);

        	message(REGION2, carnumber, cardirection, 2);

       		V(quadrant[NE]);

        	P(quadrant[SW]);

        	message(REGION3, carnumber, cardirection, 2);

        	V(quadrant[NW]);

        	message(LEAVING, carnumber, cardirection, 2);

        	V(quadrant[SW]);
        }
        else if (directions[cardirection] == "W") {

        	/*Approaching from: west
        	  Going: north*/

        	P(quadrant[SW]);
        	
        	message(REGION1, carnumber, cardirection, 0);

        	V(quadrant_gate_sem[SW]);

        	P(quadrant[SE]);

        	message(REGION2, carnumber, cardirection, 0);

       		V(quadrant[SW]);

        	P(quadrant[NE]);

        	message(REGION3, carnumber, cardirection, 0);

        	V(quadrant[SE]);

        	message(LEAVING, carnumber, cardirection, 0);

        	V(quadrant[NE]);
        }

		V(intersection_sem);        
}


/*
 * turnright()
 *
 * Arguments:
 *      unsigned long cardirection: the direction from which the car
 *              approaches the intersection.
 *      unsigned long carnumber: the car id number for printing purposes.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      This function should implement making a right turn through the 
 *      intersection from any direction.
 *      Write and comment this function.
 */

static
void
turnright(unsigned long cardirection,
          unsigned long carnumber)
{
        P(intersection_sem);

        if (directions[cardirection] == "N") {

        	/*Approaching from: north
        	  Going: west*/

        	P(quadrant[NW]);
        	
        	message(REGION1, carnumber, cardirection, 3);

        	V(quadrant_gate_sem[NW]);

        	message(LEAVING, carnumber, cardirection, 3);

        	V(quadrant[NW]);
        }
        else if (directions[cardirection] == "S") {

        	/*Approaching from: south
        	  Going: east*/

        	P(quadrant[SE]);
        	
        	message(REGION1, carnumber, cardirection, 1);

        	V(quadrant_gate_sem[SE]);

        	message(LEAVING, carnumber, cardirection, 1);

        	V(quadrant[SE]);
        }
        else if (directions[cardirection] == "E") {		
        	
        	/*Approaching from: east
        	  Going: north*/

        	P(quadrant[NE]);
        	
        	message(REGION1, carnumber, cardirection, 0);

        	V(quadrant_gate_sem[NE]);

        	message(LEAVING, carnumber, cardirection, 0);

        	V(quadrant[NE]);
        }
        else if (directions[cardirection] == "W") {

        	/*Approaching from: west
        	  Going: south*/

        	P(quadrant[SW]);
        	
        	message(REGION1, carnumber, cardirection, 2);

        	V(quadrant_gate_sem[SW]);

        	message(LEAVING, carnumber, cardirection, 2);

        	V(quadrant[SW]);


        }

		V(intersection_sem);        
}

/* synchronize_queues_and_entering()
 *
 *Arguments:
 *
 *Returns:
 *	   nothing.
 *
 *
 *Notes:
 *		this function is responsible for ensuring cars arriving from the same
 *		direction enter the intersection in order. This is achieved by properly
 *		synchronizing the FIFO/queues for each direction (N,E,S,W). We first
 *		check if the queue has any cars waiting (sleeping) on it and then check
 *		if the 'head' thread (i.e the car that arrived first) is sleeping. We want
 *		it to be sleeping because if it is awake and is not entering the intersection
 *		then it is not synchronized properly (besides, the queue checks for any sleepers
 *		and wakes the first element). We keep repeating this process until all the NCARS
 *		cars have entered and left. 
 *
 *
 *
 */

static
void
synchronize_queues_and_entering()

{
		while(cars_directed < NCARS){

			// NORTH-WEST QUEUE: for cars approaching from the North

			P(quadrant_queue_sem[NW]); // synchronize since the queue is a shared data structure

			//check if queue has any threads in it

			if (q_empty(quadrant_queue[NW]) == 0){

				//check if the head is sleeping

				int spl = splhigh();

				struct thread * head_ptr = q_getguy(quadrant_queue[NW], q_getstart(quadrant_queue[NW]));

				int head_is_sleeping = thread_hassleepers(head_ptr);

				splx(spl);

				if (head_is_sleeping){

					head_ptr = q_remhead(quadrant_queue[NW]);

					V(quadrant_queue_sem[NW]); //release the semaphore when entering

					P(quadrant_gate_sem[NW]);

					//allow the car to enter

					int spl2 = splhigh();

					thread_wakeup(head_ptr);

					splx(spl2);

				}
				else{
					V(quadrant_queue_sem[NW]); //make sure to increment this back on if the queue is empty for 
								 //future cases.
				}

			}
			else {
				V(quadrant_queue_sem[NW]); //make sure to increment this back on if the queue is empty for 
								 //future cases.
			}


				
			// SOUTH-WEST QUEUE: for cars approaching from the West

			P(quadrant_queue_sem[SW]);

			if(q_empty(quadrant_queue[SW]) == 0){


				int spl = splhigh();

				struct thread* head_ptr = q_getguy(quadrant_queue[SW], q_getstart(quadrant_queue[SW]));

				int head_is_sleeping = thread_hassleepers(head_ptr);

				splx(spl);

				if (head_is_sleeping){

					head_ptr = q_remhead(quadrant_queue[SW]);

					V(quadrant_queue_sem[SW]);

					P(quadrant_gate_sem[SW]);

					int spl2 = splhigh();

					thread_wakeup(head_ptr);

					splx(spl2);

				}
				else {
					V(quadrant_queue_sem[SW]);
				}
			}
			else {
				V(quadrant_queue_sem[SW]);
			}
				

			// SOUTH-EAST QUEUE: for cars approaching from the South

			P(quadrant_queue_sem[SE]);

			if(q_empty(quadrant_queue[SE]) == 0){


				int spl = splhigh();

				struct thread* head_ptr = q_getguy(quadrant_queue[SE], q_getstart(quadrant_queue[SE]));

				int head_is_sleeping = thread_hassleepers(head_ptr);

				splx(spl);

				if (head_is_sleeping){

					head_ptr = q_remhead(quadrant_queue[SE]);

					V(quadrant_queue_sem[SE]);

					P(quadrant_gate_sem[SE]);

					int spl2 = splhigh();

					thread_wakeup(head_ptr);

					splx(spl2);

				}
				else {
					V(quadrant_queue_sem[SE]);
				}

			}
			else {
				V(quadrant_queue_sem[SE]);
			}


				

			// NORTH-EAST QUEUE: for cars approaching from the East

			P(quadrant_queue_sem[NE]);

			if(q_empty(quadrant_queue[NE]) == 0){


				int spl = splhigh();

				struct thread* head_ptr = q_getguy(quadrant_queue[NE], q_getstart(quadrant_queue[NE]));

				int head_is_sleeping = thread_hassleepers(head_ptr);

				splx(spl);

				if (head_is_sleeping){

					head_ptr = q_remhead(quadrant_queue[NE]);

					V(quadrant_queue_sem[NE]);

					P(quadrant_gate_sem[NE]);

					int spl2 = splhigh();

					thread_wakeup(head_ptr);

					splx(spl2);

				}
				else{
					V(quadrant_queue_sem[NE]);
				}

			}
			else {
				V(quadrant_queue_sem[NE]);
			}

			thread_yield();
		}
}


/*
 * approachintersection()
 *
 * Arguments: 
 *      void * unusedpointer: currently unused.
 *      unsigned long carnumber: holds car id number.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Change this function as necessary to implement your solution. These
 *      threads are created by createcars().  Each one must choose a direction
 *      randomly, approach the intersection, choose a turn randomly, and then
 *      complete that turn.  The code to choose a direction randomly is
 *      provided, the rest is left to you to implement.  Making a turn
 *      or going straight should be done by calling one of the functions
 *      above.
 */
 
static
void
approachintersection(void * unusedpointer,
                     unsigned long carnumber)
{
        int cardirection, turn;

        /*
         * Avoid unused variable and function warnings.
         */

        (void) unusedpointer;

        /*
         * cardirection is set randomly.
         */

        cardirection = random() % 4;
        turn = random() % 3;

        int destdirection = find_destination_direction(cardirection, turn);

        if (directions[cardirection] == "N") {

        	P(quadrant_queue_sem[NW]);

        	q_addtail(quadrant_queue[NW], curthread);

        	message(APPROACHING, carnumber, cardirection, destdirection);

        	V(quadrant_queue_sem[NW]);
        }
        
        else if (directions[cardirection] == "S") {

        	P(quadrant_queue_sem[SE]);

        	q_addtail(quadrant_queue[SE], curthread);

        	message(APPROACHING, carnumber, cardirection, destdirection);

        	V(quadrant_queue_sem[SE]);
        }

        else if (directions[cardirection] == "E") {

        	P(quadrant_queue_sem[NE]);

        	q_addtail(quadrant_queue[NE], curthread);

        	message(APPROACHING, carnumber, cardirection, destdirection);

        	V(quadrant_queue_sem[NE]);
        }

        else if (directions[cardirection] == "W") {

        	P(quadrant_queue_sem[SW]);

        	q_addtail(quadrant_queue[SW], curthread);

        	message(APPROACHING, carnumber, cardirection, destdirection);

        	V(quadrant_queue_sem[SW]);
        }

        int spl = splhigh();
		
		thread_sleep(curthread);

		splx(spl);

		if (turn_directions[turn] == "straight") {

			gostraight(cardirection, carnumber);

		}
		else if (turn_directions[turn] == "left") {
			
			turnleft(cardirection, carnumber);

		}
		else if (turn_directions[turn] == "right") {
			
			turnright(cardirection, carnumber);

		}

		P(car_counter_sem);

		cars_directed++;

		V(car_counter_sem);
}


/*
 * createcars()
 *
 * Arguments:
 *      int nargs: unused.
 *      char ** args: unused.
 *
 * Returns:
 *      0 on success.
 *
 * Notes:
 *      Driver code to start up the approachintersection() threads.  You are
 *      free to modiy this code as necessary for your solution.
 */

int
createcars(int nargs,
           char ** args)
{
        int index, error;

        /*
         * Avoid unused variable warnings.
         */

        (void) nargs;
        (void) args;

        cars_directed = 0;

        /*Initialize global data structures.*/
        
        quadrant[NW] = sem_create("NW_quadrant_sem", 1);
        quadrant[NE] = sem_create("NE_quadrant_sem", 1);        
        quadrant[SE] = sem_create("SE_quadrant_sem", 1);
        quadrant[SW] = sem_create("SW_quadrant_sem", 1);

        quadrant_queue_sem[NW] = sem_create("NW_quadrant_queue_sem", 1);
        quadrant_queue_sem[NE] = sem_create("NE_quadrant_queue_sem", 1);
        quadrant_queue_sem[SE] = sem_create("SE_quadrant_queue_sem", 1);
        quadrant_queue_sem[SW] = sem_create("SW_quadrant_queue_sem", 1);

        quadrant_gate_sem[NW] = sem_create("NW_quadrant_gate_sem", 1);
        quadrant_gate_sem[NE] = sem_create("NE_quadrant_gate_sem", 1);
        quadrant_gate_sem[SE] = sem_create("SE_quadrant_gate_sem", 1);
        quadrant_gate_sem[SW] = sem_create("SW_quadrant_gate_sem", 1);

        intersection_sem = sem_create("intersection_sem", 3);
        car_counter_sem = sem_create("count_cars", 1);

        quadrant_queue[NW] = q_create(NCARS);
        quadrant_queue[NE] = q_create(NCARS);
        quadrant_queue[SE] = q_create(NCARS);
        quadrant_queue[SW] = q_create(NCARS);

        /*
         * Start NCARS approachintersection() threads.
         */

        for (index = 0; index < NCARS; index++) {

                error = thread_fork("approachintersection thread",
                                    NULL,
                                    index,
                                    approachintersection,
                                    NULL
                                    );

                /*
                 * panic() on error.
                 */

                if (error) {
                        
                        panic("approachintersection: thread_fork failed: %s\n",
                              strerror(error)
                              );
                }
        }

        synchronize_queues_and_entering();

        sem_destroy(quadrant[NW]);
        sem_destroy(quadrant[NE]);        
        sem_destroy(quadrant[SE]);
        sem_destroy(quadrant[SW]);

        sem_destroy(quadrant_queue_sem[NW]);
        sem_destroy(quadrant_queue_sem[NE]);        
        sem_destroy(quadrant_queue_sem[SE]);
        sem_destroy(quadrant_queue_sem[SW]);        

        sem_destroy(quadrant_gate_sem[NW]);
        sem_destroy(quadrant_gate_sem[SW]);
        sem_destroy(quadrant_gate_sem[SE]);
        sem_destroy(quadrant_gate_sem[NE]);

        sem_destroy(intersection_sem);
        sem_destroy(car_counter_sem);

        q_destroy(quadrant_queue[NW]);
        q_destroy(quadrant_queue[NE]);
        q_destroy(quadrant_queue[SE]);
        q_destroy(quadrant_queue[SW]);

        return 0;
}
