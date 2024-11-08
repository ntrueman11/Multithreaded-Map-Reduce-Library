#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "threadpool.h"
#include <sys/stat.h>
#include "mapreduce.h"
#include <ctype.h>

// Globals for Partitions
Partition *partition;
unsigned int num_partitions;
Dictionary **curr_partition_arr;

/**
 * Compares the file sizes of two File structures.
 * Parameters:
 * a - Pointer to the first File structure.
 * b - Pointer to the second File structure.
 * Return:
 * Positive if file size of a > b, negative if a < b, 0 if equal.
 */
int compare_size(const void *a, const void *b)
{
    File *file = (File *)a;
    File *file2 = (File *)b;
    return (file->file_size - file2->file_size);
}

/**
* Run the MapReduce framework
* Parameters:
*     file_count   - Number of files (i.e. input splits)
*     file_names   - Array of filenames
*     mapper       - Function pointer to the map function
*     reducer      - Function pointer to the reduce function
*     num_workers  - Number of threads in the thread pool
*     num_parts    - Number of partitions to be created
*/
void MR_Run(unsigned int file_count, char *file_names[], Mapper mapper, Reducer reducer, unsigned int num_workers, unsigned int num_parts) {
    num_partitions = num_parts;
    partition = malloc(sizeof(Partition) * num_partitions);
    curr_partition_arr = malloc(sizeof(Dictionary*)*num_partitions);
    for (unsigned int i = 0; i < num_partitions; i++) {
        partition[i].head = NULL;
        partition[i].tail = NULL;
        partition[i].position = NULL;
        pthread_mutex_init(&partition[i].mutex, NULL);
    }

    // Initialize the thread pool
    ThreadPool_t *tp = ThreadPool_create(num_workers);

    struct stat file_stat;
    File *file = malloc(file_count * sizeof(File));
    for (unsigned int i = 0; i < file_count; i++)
    {
        file[i].filename = file_names[i];

        // Get the size of the file using stat
        if (stat(file_names[i], &file_stat) == 0)
        {
            file[i].file_size= file_stat.st_size;
        }
        else
        {
            file[i].file_size = 0; 
        }
    }
    qsort(file, file_count, sizeof(File), compare_size);
    // Submit map jobs to the thread pool in sorted order
    for (unsigned int i = 0; i < file_count; i++) {
        ThreadPool_add_job(tp, (thread_func_t)mapper, file[i].filename);
    }
    ThreadPool_check(tp);


    for (unsigned int i = 0; i < num_partitions; i++) {
        MR_Reduce_Args *args = malloc(sizeof(MR_Reduce_Args));
        args->partition_idx = i;
        args->reducer = reducer;
        ThreadPool_add_job(tp, MR_Reduce, args);
    }
    ThreadPool_destroy(tp);


    // Clean up partitions and mutexes
    for (unsigned int i = 0; i < num_partitions; i++) {
        pthread_mutex_destroy(&partition[i].mutex);
        Dictionary *current = partition[i].head;
        while (current) {
            Dictionary *temp = current;
            current = current->next;
            free(temp->key);
            free(temp->value);
            free(temp);
        }
    }
    free(file);
    free(partition);
    free(curr_partition_arr);
}

/**
* Write a specifc map output, a <key, value> pair, to a partition
* Parameters:
*     key           - Key of the output
*     value         - Value of the output
*/
void MR_Emit(char *key, char *value) {

    unsigned int index = MR_Partitioner(key, num_partitions);
    pthread_mutex_lock(&partition[index].mutex);

    Dictionary *dict = malloc(sizeof(Dictionary));

    dict->key = strdup(key); 
    dict->value = strdup(value);
    dict->next = NULL;

    // // Insert the new node at the end of the list
    if (partition[index].head == NULL) {
        partition[index].head = dict;
        partition[index].tail = dict;
    } else {
        partition[index].tail->next = dict;
        partition[index].tail = dict;
    }

    pthread_mutex_unlock(&partition[index].mutex);
}

/**
* Hash a mapper's output to determine the partition that will hold it
* Parameters:
*     key           - Key of a specifc map output
*     num_partitions- Total number of partitions
* Return:
*     unsigned int  - Index of the partition
*/
unsigned int MR_Partitioner(char *key, unsigned int num_partitions) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key++) != '\0')
    hash = hash * 33 + c;
    return hash % num_partitions;
}


/**
* Run the reducer callback function for each <key, (list of values)> 
* retrieved from a partition
* Parameters:
*     threadarg     - Pointer to a hidden args object
*/
void MR_Reduce(void *threadarg) {
    
    //Initilize from args
    MR_Reduce_Args *args = (MR_Reduce_Args *)threadarg;
    unsigned int partition_idx = args->partition_idx;
    Reducer reducer = args->reducer;
    curr_partition_arr[partition_idx] = partition[partition_idx].head; // Start from head

    while (curr_partition_arr[partition_idx] != NULL) {
        char *key = curr_partition_arr[partition_idx]->key;
        reducer(key, partition_idx);

        // Move to the next unique key
        while (curr_partition_arr[partition_idx] != NULL && strcmp(curr_partition_arr[partition_idx]->key, key) == 0) {
            curr_partition_arr[partition_idx] = curr_partition_arr[partition_idx]->next;
        }
    }

    free(args);
}

/**
* Get the next value of the given key in the partition
* Parameters:
*     key           - Key of the values being reduced
*     partition_idx - Index of the partition containing this key
* Return:
*     char *        - Value of the next <key, value> pair if its key is the current key
*     NULL          - Otherwise
*/
char *MR_GetNext(char *key, unsigned int partition_idx) {
    Dictionary *current_node = curr_partition_arr[partition_idx];

    while (current_node != NULL) {
        if (strcmp(current_node->key, key) == 0) {
            curr_partition_arr[partition_idx] = current_node->next;
            return strdup(current_node->value); //Duplicate to solve Mem leak
        } else if (strcmp(current_node->key, key) > 0) {
            return NULL;
        }
        current_node = current_node->next;
    }
    return NULL;
    
}
