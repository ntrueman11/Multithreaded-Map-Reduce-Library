// threadpool.c
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

typedef void (*thread_func_t)(void *arg);

typedef struct ThreadPool_job_t {
    thread_func_t func;              // function pointer
    void *arg;                       // arguments for that function
    struct ThreadPool_job_t *next;   // pointer to the next job in the queue
} ThreadPool_job_t;

typedef struct {
    unsigned int size;               // jobs in the queue
    ThreadPool_job_t *head;          // pointer to the first (shortest) job
    ThreadPool_job_t *tail;          // pointer to the last job
} ThreadPool_job_queue_t;

typedef struct {
    pthread_t *threads;              // pointer to the array of thread handles
    ThreadPool_job_queue_t jobs;     // queue of jobs waiting for a thread to run
    pthread_mutex_t job_mutex;       // job mutex lock
    pthread_cond_t job_cond;         // job condition 
    unsigned int num_threads;        // number of threads 
    bool destroy_flag;               // destroy flag
} ThreadPool_t;

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
    pthread_mutex_init(&tp->job_mutex, NULL);
    pthread_cond_init(&tp->job_cond, NULL);
    
    tp->num_threads = num;
    tp->destroy_flag = false;

    for (unsigned int i = 0; i < num; i++) {
        pthread_create(&tp->threads[i], NULL, Thread_run, tp);
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

    //Lock mutex then signal to remaining threads to exit waiting
    pthread_mutex_lock(&tp->job_mutex);
    tp->destroy_flag = true;
    pthread_cond_broadcast(&tp->job_cond);
    pthread_mutex_unlock(&tp->job_mutex);

    //Wait for threads to terminate
    for (unsigned int i = 0; i < tp->num_threads; i++) {
        pthread_join(tp->threads[i], NULL);
    }
    free(tp->threads);
    
    //Ensure jobs removed (Not sure if need cuz of check ?)
    // ThreadPool_job_t *job = tp->jobs.head;
    // while (job) {
    //     ThreadPool_job_t *temp_job = job;
    //     job = job->next;
    //     free(temp_job);
    // }

    //Destroy Mutex, condition and free memory
    pthread_mutex_destroy(&tp->job_mutex);
    pthread_cond_destroy(&tp->job_cond);
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

    pthread_mutex_lock(&tp->job_mutex);

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
    pthread_cond_signal(&tp->job_cond);
    pthread_mutex_unlock(&tp->job_mutex);

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
    pthread_mutex_lock(&tp->job_mutex);

    //Wait while no available jobs
    while (tp->jobs.size == 0 && !tp->destroy_flag) {
        pthread_cond_wait(&tp->job_cond, &tp->job_mutex);
    }
    //Flag set to destroy threadpool
    if (tp->destroy_flag && tp->jobs.size == 0) {
        pthread_mutex_unlock(&tp->job_mutex);
        return NULL;
    }

    ThreadPool_job_t *job = tp->jobs.head;
    // Set current head to next job
    if (job != NULL) {
        tp->jobs.head = job->next;
        tp->jobs.size--;
    }
    //Signal for Check when size is empty
    if (tp->jobs.size == 0) {
        pthread_cond_signal(&tp->job_cond);
    }

    pthread_mutex_unlock(&tp->job_mutex);
    return job;
}

/**
* Start routine of each thread in the ThreadPool Object
* In a loop, check the job queue, get a job (if any) and run it
* Parameters:
*     tp - Pointer to the ThreadPool object containing this thread
*/
void *Thread_run(ThreadPool_t *tp) {
    //Breaks once no more jobs are available
    ThreadPool_job_t *job;
    while ((job = ThreadPool_get_job(tp)) != NULL) {
        job->func(job->arg);
        free(job);
    }
    return NULL;
}

/**
* Ensure that all threads are idle and the job queue is empty before returning
* Parameters:
*     tp - Pointer to the ThreadPool object that will be destroyed
*/
void ThreadPool_check(ThreadPool_t *tp) {

    pthread_mutex_lock(&tp->job_mutex);
    //Wait until signal is sent 
    while (tp->jobs.size > 0) {
        pthread_cond_wait(&tp->job_cond, &tp->job_mutex);
    }
    pthread_mutex_unlock(&tp->job_mutex);
}