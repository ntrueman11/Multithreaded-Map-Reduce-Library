#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include "threadpool.h"


/**
* Wrapper function for running a thread.
* Parameters:
*     arg - Pointer to the ThreadPool_t struc.
*/
void *Run_Wrapper(void *arg) {
    return Thread_run((ThreadPool_t *) arg);
}
/**
* C style constructor for creating a new ThreadPool object
* Parameters:
*     num - Number of threads to create
* Return:
*     ThreadPool_t* - Pointer to the newly created ThreadPool object
*/
ThreadPool_t *ThreadPool_create(unsigned int num) {
    
    // Memory Allocation
    ThreadPool_t *tp = malloc(sizeof(ThreadPool_t));
    if (tp == NULL) return NULL;

    tp->threads = malloc(sizeof(pthread_t) * num);
    if (tp->threads == NULL) {
        free(tp);
        return NULL;
    }
    // No current jobs
    tp->jobs.head = NULL;
    tp->jobs.size = 0;
    // Initialize Mutex Lock and condition
    pthread_mutex_init(&tp->mutex, NULL);
    pthread_cond_init(&tp->condition, NULL);
    pthread_cond_init(&tp->check_condition, NULL);
    
    tp->num_threads = num;
    tp->destroy_flag = false;

    for (unsigned int i = 0; i < num; i++) {
        pthread_create(&tp->threads[i], NULL, Run_Wrapper, tp);
    }

    return tp;
}

/**
* C style destructor to destroy a ThreadPool object
* Parameters:
*     tp - Pointer to the ThreadPool object to be destroyed
*/
void ThreadPool_destroy(ThreadPool_t *tp) {

    ThreadPool_check(tp);

    //Signal to remaining threads to exit waiting
    tp->destroy_flag = true;
    pthread_cond_broadcast(&tp->condition);

    //Wait for threads to terminate
    for (unsigned int i = 0; i < tp->num_threads; i++) {
        pthread_join(tp->threads[i], NULL);
    }
    free(tp->threads);
    
    //Destroy Mutex, condition and free memory
    pthread_mutex_destroy(&tp->mutex);
    pthread_cond_destroy(&tp->condition);
    pthread_cond_destroy(&tp->check_condition);
    free(tp);
}

/**
* Add a job to the ThreadPool's job queue
* Parameters:
*     tp   - Pointer to the ThreadPool object
*     func - Pointer to the function that will be called by the serving thread
*     arg  - Arguments for that function
* Return:
*     true  - On success
*     false - Otherwise
*/
bool ThreadPool_add_job(ThreadPool_t *tp, thread_func_t func, void *arg) {
    ThreadPool_job_t *job = malloc(sizeof(ThreadPool_job_t));
    if (job == NULL) return false;

    job->func = func;
    job->arg = arg;
    job->next = NULL;

    pthread_mutex_lock(&tp->mutex);

    if (tp->jobs.head == NULL) {
        // Only 1 job in the queue, head and tail point to the same job
        tp->jobs.head = job;
        tp->jobs.tail = job;
    } 
     else {
        // Send the new job to the end of the queue
        tp->jobs.tail->next = job;
        tp->jobs.tail = job;
    }
    tp->jobs.size++;
    // Signaling condition and unlock since new Job available
    pthread_cond_signal(&tp->condition);
    pthread_mutex_unlock(&tp->mutex);

    return true;
}

/**
* Get a job from the job queue of the ThreadPool object
* Parameters:
*     tp - Pointer to the ThreadPool object
* Return:
*     ThreadPool_job_t* - Next job to run
*/
ThreadPool_job_t *ThreadPool_get_job(ThreadPool_t *tp) {
    ThreadPool_job_t *job = tp->jobs.head;
    // Set current head to next job
    if (job != NULL) {
        tp->jobs.head = job->next;
        tp->jobs.size--;
    }
    //Signal for Check when size is empty
    if (tp->jobs.size == 0) {
        pthread_cond_signal(&tp->check_condition);
    }

    pthread_mutex_unlock(&tp->mutex);
    return job;
}

/**
* Start routine of each thread in the ThreadPool Object
* In a loop, check the job queue, get a job (if any) and run it
* Parameters:
*     tp - Pointer to the ThreadPool object containing this thread
*/
void *Thread_run(ThreadPool_t *tp) {
    while (true) {
        ThreadPool_job_t *job;

        pthread_mutex_lock(&tp->mutex);
        
        // Wait until there’s a job available
        while (tp->jobs.size == 0 && !tp->destroy_flag) {
            pthread_cond_wait(&tp->condition, &tp->mutex);
        }
        
        // Check if we should exit due to destroy_flag
        if (tp->destroy_flag && tp->jobs.size == 0) {
            pthread_mutex_unlock(&tp->mutex);
            break;
        }

        job = ThreadPool_get_job(tp);
        pthread_mutex_unlock(&tp->mutex);
        
        if (job != NULL) {
            job->func(job->arg);
            free(job);
        }
    }
    return NULL;
}

/**
* Ensure that all threads are idle and the job queue is empty before returning
* Parameters:
*     tp - Pointer to the ThreadPool object that will be destroyed
*/
void ThreadPool_check(ThreadPool_t *tp) {

    pthread_mutex_lock(&tp->mutex);
    //Wait until signal is sent 
    while (tp->jobs.size > 0) {
        pthread_cond_wait(&tp->check_condition, &tp->mutex);
    }
    pthread_mutex_unlock(&tp->mutex);
}
