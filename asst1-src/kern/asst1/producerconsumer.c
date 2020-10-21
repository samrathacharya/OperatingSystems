/* This file will contain your solution. Modify it as you wish. */
#include <types.h>
#include <lib.h>
#include <synch.h>
#include "producerconsumer_driver.h"

/* Declare any variables you need here to keep track of and
   synchronise your bounded. A sample declaration of a buffer is shown
   below. It is an array of pointers to items.

   You can change this if you choose another implementation.
   However, you should not have a buffer bigger than BUFFER_SIZE
*/

data_item_t * item_buffer[BUFFER_SIZE];
//Condition variables
struct semaphore *full;     //Initial value 0, indicates number of items in buffer   
struct semaphore *empty;    //Initial value 
struct semaphore *mutex;    //For mutually exclusive access
int head;       //Head index
int tail;       //Tail index



/* consumer_receive() is called by a consumer to request more data. It
   should block on a sync primitive if no data is available in your
   buffer. It should not busy wait! */

data_item_t * consumer_receive(void)
{
        data_item_t * item;

        //Block if buffer is empty
        P(full);
        //semaphore for critical region
        P(mutex);
        //Get item to remove
        item = item_buffer[head];
        //Increment head by 1 for circular buffer
        head = (head+1) %BUFFER_SIZE;
        //Release mutex
        V(mutex);
        //Signal to producers
        V(empty);
        return item;
}

/* procucer_send() is called by a producer to store data in your
   bounded buffer.  It should block on a sync primitive if no space is
   available in your buffer. It should not busy wait!*/

void producer_send(data_item_t *item)
{
        // Block if buffer is full
        P(empty);
        //semaphore for critical region
        P(mutex);
        //Increment tail by 1 for circular buffer
        tail = (tail+1)%BUFFER_SIZE;
        //Add item to end
        item_buffer[tail] = item;
        //Release mutex
        V(mutex);
        //Signal to consumers 
        V(full);
}




/* Perform any initialisation (e.g. of global data) you need
   here. Note: You can panic if any allocation fails during setup */

void producerconsumer_startup(void)
{
        //Initialise semaphores for mutex
        empty = sem_create("empty", BUFFER_SIZE);
        full = sem_create("full", 0);
        mutex = sem_create("mutex", 1);
        if(empty == NULL || full==NULL || mutex==NULL){
                panic("Semaphore initialisation failed");
        }
        //Intitialisation values for circular buffer
        head = 0;
        tail = -1;     
}

/* Perform any clean-up you need here */
void producerconsumer_shutdown(void)
{
        sem_destroy(empty);
        sem_destroy(full);
        sem_destroy(mutex);
}
