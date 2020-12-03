/*
 * Describe the project.
 */
#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>
#include <string.h>
#include "project3.h"

/*
 * This function will attempt to enqueue the topic entry TE into the topic queue with name TopicID. Note that this
 * function contains a critical section and will block until the topic queue with name TopicID's mutex is unlocked.
*/
void *enqueue(void *args) {
    struct publisherArgs *pubArgs = (struct publisherArgs *)args;
    char *topicID = pubArgs->topicID;
    struct topicEntry post = pubArgs->post;
    topicQueue *topic = getQueue(topicID);
    pthread_mutex_lock(&topic->mutex);
    while (topic->totalCapacity == topic->bufferEntries) {
        // TODO: Change to a semaphore instead of looping.
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
    struct subscriberArgs *subArgs = (struct subscriberArgs*)args;
    int newEntry = *(subArgs->lastEntry) + 1;
    topicQueue *topic = getQueue(subArgs->topicID);
    if (!topic->bufferEntries) {
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
            return (void *)EXIT_SUCCESS;
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
    return (void *)EXIT_FAILURE;
}

int main() {
    char *topics[MAXTOPICS] = {"fruit", "veggies", "mushrooms", "nuts"};
    for (int i = 0; i < MAXTOPICS; i++) {
        topicStore[i] = newTopicQueue(topics[i]);
    }
    pthread_t publisher_thread;
    pthread_t clean;
    struct publisherArgs pubArgs;
    for (int i = 0; i < 5; i++) {
        pubArgs.topicID = topics[0];
        pubArgs.post = newTopicEntry("repeat", "cap");
        pthread_create(&publisher_thread, NULL, enqueue, &pubArgs);
        pthread_create(&clean, NULL, dequeue, topics[0]);
        pthread_join(publisher_thread, NULL);
        pthread_join(clean, NULL);
    }
    printf("hello");
    struct topicEntry placeholder;
    int a = 0;
    struct subscriberArgs subArgs = {.lastEntry = &a, .topicID = topics[0], .post = &placeholder};

    for (int i = 0; i < 10; i++) {
        getEntry(&subArgs);
        viewPost(placeholder);
    }
    return 0;
}
