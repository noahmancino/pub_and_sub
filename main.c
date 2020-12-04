/*
 * Describe the project.
 */
#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>
#include <string.h>
#include "project3.h"
// TODO: stop using structs as arguments to enqueue and getEntry. They are no longer start routines for pthreads.
/*
 * This function will attempt to enqueue the topic entry TE into the topic queue with name TopicID. Note that this
 * function contains a critical section and will block until the topic queue with name TopicID's mutex is unlocked.
*/
int enqueue(struct enqueueArgs *pubArgs) {
    char *topicID = pubArgs->topicID;
    struct topicEntry post = pubArgs->post;
    topicQueue *topic = getQueue(topicID);
    pthread_mutex_lock(&topic->mutex);
    while (topic->totalCapacity == topic->bufferEntries) {
        // TODO: Change to a semaphore instead of doing this the lazy way.
        pthread_mutex_unlock(&topic->mutex);
        sched_yield();
        pthread_mutex_lock(&topic->mutex);
    }
    ++topic->totalPastPosts;
    gettimeofday(&post.timeStamp, NULL);
    post.entryNum = topic->totalPastPosts, post.pubID = topic->totalPastPosts;
    topic->buffer[topic->head] = post;
    topic->head = (topic->head + 1) % topic->totalCapacity;
    ++topic->bufferEntries;
    pthread_mutex_unlock(&topic->mutex);
    return EXIT_SUCCESS;
}


/*
 * This is the start routine for a publisher thread. It takes an array of arguments to enqueue and calls enqueue on them.
 */
void *publisher(void *args) {
    struct publisherArg *pubArgs = (struct publisherArg *)args;
    for (int i = 0; i < pubArgs->numArgs; i++) {
        enqueue(&pubArgs->eArgs[i]);
    }
    return EXIT_SUCCESS;
}


/*
 * This function will attempt to dequeue a post from the topic queue named topicID if the oldest post is older than
 * DELTA. Note that this function contains a critical section and will block until the topic queue with name topicID's
 * mutex is unlocked.
*/
void *dequeue(void *topicID) {
    topicQueue *topic = getQueue((char *)topicID);
    pthread_mutex_lock(&topic->mutex);
    if (!topic->bufferEntries) {
        pthread_mutex_unlock(&topic->mutex);
        return (void *) EXIT_FAILURE;
    }
    struct topicEntry dequeued = topic->buffer[topic->tail];
    struct timeval delta;
    gettimeofday(&delta, NULL);
    delta.tv_sec -= dequeued.timeStamp.tv_sec;
    delta.tv_usec -= dequeued.timeStamp.tv_usec;
    if (delta.tv_sec || delta.tv_usec > DELTA) {
        topic->tail = (topic->tail + 1) % topic->totalCapacity;
        --topic->bufferEntries;
    }
    pthread_mutex_unlock(&topic->mutex);
    return EXIT_SUCCESS;
}

/*
 * The start routine for the cleaner thread. Attempts to dequeue from each topic each time DELTA milliseconds have
 * elapsed.
 */
void *cleaner(void *arg) {
    unsigned long int topics = (unsigned long int) arg;
    while (1) {
        for (int i = 0; i < topics; i++) {
            dequeue(topicStore[i].name);
        }
        usleep(100);
    }
    return EXIT_SUCCESS;
}

/*
int getEntry(int *lastEntry, struct topicEntry *post, char *topicID) {
    int newEntry = *lastEntry + 1;
    topicQueue *topic = getQueue(topicID);
    if (!topic->bufferEntries) {
        return 0;
    }
    pthread_mutex_lock(&topic->mutex);
    // If the next entry isn't in the queue, we need to know if there is a younger/later entry or not.
    int was_dequed = 0;
    for (int i = 0; i <= topic->bufferEntries; i++) {
        struct topicEntry entry = topic->buffer[(topic->tail + i) % topic->totalCapacity];
        if (entry.entryNum == newEntry) {
            // Case where newEntry is in the queue
            pthread_mutex_unlock(&topic->mutex);
            memcpy(post, &entry, sizeof(entry));
            *lastEntry = newEntry;
            return 0;
        }
        if (entry.entryNum > newEntry) {
            was_dequed = 1;
        }
    }
    if (was_dequed) {
        // Case where newEntry is not in the queue but later/younger entries are.
        struct topicEntry oldest = topic->buffer[topic->tail];
        pthread_mutex_unlock(&topic->mutex);
        memcpy(post, &oldest, sizeof(oldest));
        *lastEntry = oldest.entryNum;
        return 1;
    }
    // Case where neither newEntry or anything younger than newEntry has been placed in the queue.
    printf("here!\n");
    pthread_mutex_unlock(&topic->mutex);
    return 0;
}
*/
/*
 * This routine will take three arguments: A) An integer argument lastEntry which is the number of the last entry read
 * by the calling thread on this topic, B) A reference to an empty topicEntry struct, and C) a topicID. The routine will
 * attempt to get the lastEntry+1 entry if it is in the topic queue, or the oldest entry younger than lastEntry+1 if
 * not.
*/
void *getEntry(void *args) {
    struct getEntryArgs *subArgs = (struct getEntryArgs*)args;
    int newEntry = *(subArgs->lastEntry) + 1;
    topicQueue *topic = getQueue(subArgs->topicID);
    if (!topic->bufferEntries) {
        printf("a different kind of failure\n");
        return (void *)EXIT_FAILURE;
    }
    pthread_mutex_lock(&topic->mutex);
    // If the next entry isn't in the queue, we need to know if there is a younger/later entry or not.
    int was_dequed = 0;
    for (int i = 0; i <= topic->bufferEntries; i++) {
        struct topicEntry entry = topic->buffer[(topic->tail + i) % topic->totalCapacity];
        if (entry.entryNum == newEntry) {
            // Case where newEntry is in the queue
            pthread_mutex_unlock(&topic->mutex);
            memcpy(subArgs->post, &entry, sizeof(entry));
            *subArgs->lastEntry = newEntry;
            return EXIT_SUCCESS;
        }
        if (entry.entryNum > newEntry) {
            was_dequed = 1;
        }
    }
    if (was_dequed) {
        // Case where newEntry is not in the queue but later/younger entries are.
        struct topicEntry oldest = topic->buffer[topic->tail];
        pthread_mutex_unlock(&topic->mutex);
        memcpy(subArgs->post, &oldest, sizeof(oldest));
        *subArgs->lastEntry = oldest.entryNum;
        return EXIT_SUCCESS;
    }
    // Case where neither newEntry or anything younger than newEntry has been placed in the queue.
    pthread_mutex_unlock(&topic->mutex);
    printf("failure ):\n");
    fflush(stdout);
    return (void *)EXIT_FAILURE;
}


/*
 * Start routine for the subscriber thread. Takes an array of getEntry args and the length of the array, calls
 * getEntry for each one, and displays the ticket it fills.
 */
void *subscriber(void *args) {
    struct subscriberArg *entryArgs = (struct subscriberArg *)args;
    int lastEntry = 0;
    for (int i = 0; i < entryArgs->numArgs; i++) {
        entryArgs->geArgs[i].lastEntry = &lastEntry;
        if (!getEntry(&entryArgs->geArgs[i])) {
            printf("not else\n");
            viewPost(*entryArgs->geArgs[i].post);
        }
        else {
            printf("else\n");
        }
    }
}

int main(int argc, char *argv[]) {
    char *masterCommandFile = argv[1];
    char ***tokenizedMaster = tokenize(masterCommandFile);
    for (int i = 0; tokenizedMaster[i] != NULL; i++) {
        // Create a topic with ID (integer) and length. This allocates a topic queue.
        if (!strcmp(tokenizedMaster[i][0], "create")) {

        }
        // Start all of the publishers and subscribers, as well as the cleanup thread.
        else if (!strcmp(tokenizedMaster[i][0], "start")) {

        }
        // Set delta (determines how long until posts get cleaned from store) to specified value.
        else if (!strcmp(tokenizedMaster[i][0], "delta")) {

        }
        // Adds a job to the publisher threads workload. A free thread is allocated to be the “proxy" for the publisher
        else if (!strcmp(tokenizedMaster[i][1], "publisher")) {

        }
        // Adds a job to the subscriber threads workload A free thread is allocated to be the “proxy" for the subscriber
        else if (!strcmp(tokenizedMaster[i][1], "subscriber")) {

        }
    }
    return 0;
}
