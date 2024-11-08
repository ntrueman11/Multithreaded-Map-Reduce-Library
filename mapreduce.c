#ifndef MAPREDUCE_H
#define MAPREDUCE_H
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "threadpool.h"
#include <sys/stat.h>

// function pointer typedefs
typedef void (*Mapper)(char *file_name);
typedef void (*Reducer)(char *key, unsigned int partition_idx);

typedef struct Dictionary {
    char *key;
    char *value;
    struct Dictionary *next; // Next node in the list
} Dictionary;

// Structure for a partition holding a linked list of Dictionary nodes and a mutex for thread safety
typedef struct {
    Dictionary *head;         // Head of the linked list
    Dictionary *tail;         // Tail of the linked list
    Dictionary *position;     // Current position
    pthread_mutex_t mutex;    // Mutex for partition
} Partition;

typedef struct {
    unsigned int partition_idx;
    Reducer reducer;
} MR_Reduce_Args;

typedef struct {
    char *file_name;
    off_t file_size;
} File;
// Globals for Partitions
Partition *partition;
unsigned int num_partitions;

// library functions that must be implemented

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
    
    partition = malloc(sizeof(Partition) * num_parts);
    for (unsigned int i = 0; i < num_parts; i++) {
        partition[i].head = NULL;
        partition[i].tail = NULL;
        partition[i].position = NULL;
        pthread_mutex_init(&partition[i].mutex, NULL);
    }

    // Initialize the thread pool
    ThreadPool_t *tp = ThreadPool_create(num_workers);

    // Array to store jobs with file sizes
    File files[file_count];
    struct stat file_stat;
    for (unsigned int i = 0; i < file_count; i++) {
        
        if (stat(file_names[i], &file_stat) == 0) {
            files[i].file_name = file_names[i];
            files[i].file_size = file_stat.st_size;
        } 
    }
    sort_files(files, file_count);

    // Submit map jobs to the thread pool in sorted order
    for (unsigned int i = 0; i < file_count; i++) {
        ThreadPool_add_job(tp, (thread_func_t)mapper, files[i].file_name);
    }

    ThreadPool_destroy(tp);


    // Create reducer threads, one for each partition
    pthread_t reducer_t[num_parts];
    for (unsigned int i = 0; i < num_parts; i++) {
        MR_Reduce_Args *args = malloc(sizeof(MR_Reduce_Args));
        args->partition_idx = i;
        args->reducer = reducer;

        pthread_create(&reducer_t[i], NULL, MR_Reduce, args);
    }

    for (unsigned int i = 0; i < num_parts; i++) {
        pthread_join(reducer_t[i], NULL);
    }

    // Clean up partitions and mutexes
    for (unsigned int i = 0; i < num_parts; i++) {
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
    free(partition);
}

/**
* Write a specifc map output, a <key, value> pair, to a partition
* Parameters:
*     key           - Key of the output
*     value         - Value of the output
*/
void MR_Emit(char *key, char *value) {

    // Determine the partition using the partitioning function
    unsigned int index = MR_Partitioner(key, num_partitions);

    pthread_mutex_lock(&partition[index].mutex);

    Dictionary *dict = malloc(sizeof(Dictionary));
    // Duplicate the key and value strings
    dict->key = strdup(key); 
    dict->value = strdup(value);
    dict->next = NULL;

    // Insert the new node at the end of the list
    if (partition[index].head == NULL) {
        partition[index].head = dict;
        partition[index].tail = dict;
    } else {
        // Send to the end
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
unsigned long MR_Partitioner(char *key, int num_partitions) {
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
    
    Partition *part = &partition[partition_idx];

    pthread_mutex_lock(&part->mutex);
    Dictionary *current_item = part->head;
    pthread_mutex_unlock(&part->mutex);

    while (current_item != NULL) {

        char *key = current_item->key;

        reducer(key, partition_idx);

        //Find next unique key to reduce
        pthread_mutex_lock(&part->mutex);
        while (current_item != NULL && strcmp(current_item->key, key) == 0) {
            current_item = current_item->next;
        }
        pthread_mutex_unlock(&part->mutex);
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
    Partition *part = &partition[partition_idx];

    pthread_mutex_lock(&part->mutex);
    
    if (part->position == NULL || strcmp(part->position->key, key) != 0) {
        //Start at head
        part->position = part->head;

        //Find postions with same key
        while (part->position != NULL && strcmp(part->position->key, key) != 0) {
            part->position = part->position->next;
        }
    }

    // No matching key
    if (part->position == NULL) {
        pthread_mutex_unlock(&part->mutex);
        return NULL;
    }

    // Store the value to return
    char *key_val = part->position->value;

    //Saves position for next call to function
    part->position = part->position->next;
    while (part->position != NULL && strcmp(part->position->key, key) != 0) {
        part->position = part->position->next;
    }

    pthread_mutex_unlock(&part->mutex);
    return key_val;
}

void sort_files(File *files, unsigned int file_count) {

    for (unsigned int i = 0; i < file_count - 1; i++) {
        for (unsigned int j = i + 1; j < file_count; j++) {
            if (files[i].file_size > files[j].file_size) {
                File temp = files[i];
                files[i] = files[j];
                files[j] = temp;
            }
        }
    }
}
#endif
