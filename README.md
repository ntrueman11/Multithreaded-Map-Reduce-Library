# - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# Name : Nathan Trueman
# SID : 1702962
# CCID : ntrueman
# - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


## Synchronization Primitizes
Mutex locks (pthread_mutex_t) and condition variables (pthread_cond_t) are used throughout the implementation to ensure safe thread access to the shared data in both the ThreadPool and MapReduce libraries. In threadpool.c, a mutex lock (tp->mutex) is used to synchronize access to the job queue, preventing concurrent modifications by threads. Additionally, each partition in mapreduce.c has its own mutex which ensures that partitions are accessed safely. This is shown particularly during the MR_Emit function where new key-value pairs are added to the linked list within each partition. Condition variables are used in ThreadPool to manage thread waiting and signaling: the condition variable signals worker threads when new jobs are available, while the check_condition variable ensures all jobs are completed before the thread pool is destroyed.

## Partition Implementation
Each partition is implemented as a linked list where each node represents a Dictionary entry containing a key, value, and a pointer to the next node, built as key-value pairs are emitted using the MR_Emit function. The partition array is an array of Partition structs, with each Partition containing head and tail pointers for managing the linked list of Dictionary nodes, a position pointer for tracking the current location during reduction, and a mutex lock for synchronizing access. Additionally, curr_partition_arr is an auxiliary array used to track the current position in each partition's linked list while iterating over values for a specific key during reduction.

## Testing 
The application was run on various sample text files, and the output was validated by comparing the word count results across multiple partitions. Valgrind was also used extensively to check for memory leaks, ensuring that all dynamically allocated memory was properly freed at the end of execution. In addition to the provided sample test files which were ran, new test files were created knowing the expected value to compare to the programs output to verify it was correct.
