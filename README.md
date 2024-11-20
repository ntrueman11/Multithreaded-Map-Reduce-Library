# Multithreaded MapReduce Library

## Overview

This project implements a basic MapReduce framework in C for a word count application. It includes three main components:
1. **`distwc.c`**: Implements the main program for word count, including `Map` and `Reduce` functions.
2. **`threadpool.c`**: Provides a thread pool implementation to manage concurrent worker threads.
3. **`mapreduce.c`**: Implements the core MapReduce logic, including partitioning, mapping, reducing, and managing partitions.

## Synchronization Primitives

To handle concurrent access to shared resources, the following synchronization primitives are used:

- **Mutex (pthread_mutex_t)**:
  - Used in `mapreduce.c` to ensure mutual exclusion when writing to partitions (`MR_Emit` function).
  - Used in `threadpool.c` to protect access to the job queue in the thread pool, ensuring that multiple threads do not modify the queue simultaneously.

- **Condition Variables (pthread_cond_t)**:
  - Used in `threadpool.c` to signal worker threads when a new job is available in the job queue.
  - Another condition variable is used to signal when all jobs have been completed, allowing the main thread to wait until all tasks are done.

These synchronization primitives ensure thread safety in both the MapReduce and thread pool implementations, preventing race conditions and allowing efficient parallel processing.

## Partitions Implementation

Each partition in the MapReduce framework is represented by a linked list of key-value pairs (or dictionary entries), implemented in `mapreduce.c`:

- **Partition Structure**:
  - Each partition is represented by a `Partition` struct, which contains:
    - A linked list (`Dictionary`) to store key-value pairs.
    - Mutexes to ensure safe concurrent access when multiple threads emit to the same partition.

- **Adding Data to Partitions**:
  - In `MR_Emit`, each key-value pair emitted by the `Map` function is stored in a partition, determined by a hash-based partitioning function.
  - The `Partition` struct maintains a linked list of `Dictionary` nodes for each unique key-value pair.
  - If a partition already has entries, the new entry is appended to the end of the linked list.

- **Accessing Partitions**:
  - In `MR_GetNext`, the `Reduce` function retrieves the next value for a specific key within a partition. A separate `position` pointer in each partition allows sequential access to values associated with the current key.

This partition structure allows the MapReduce framework to organize data into segments that can be processed concurrently in the `Reduce` phase.

## Testing and Validation

The implementation was tested using various input files and scenarios to validate its correctness and performance:

1. **Concurrency and Thread Safety**:
   - To test concurrency, multiple files of varying sizes were processed simultaneously. This ensured that the thread pool correctly distributed `Map` and `Reduce` tasks across multiple threads.
   - Valgrind was used to check for memory leaks and synchronization errors, confirming that all allocated resources were properly freed and that there were no data races.

2. **Partition Validation**:
   - The partitioning function was tested to ensure that each key-value pair was assigned to the correct partition based on the partition index. 
   - The partitioned data was reviewed to ensure that each partition contained the expected data.

3. **Error Handling**:
   - The program was tested with various error scenarios, such as non-existent input files, to confirm that it handled errors gracefully.

