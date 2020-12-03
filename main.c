/*
 * Describe the project.
 */
#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <zconf.h>

#define MAXTOPICS 4
#define URLSIZE 100
#define CAPSIZE 200
#define MAXNAME 25
#define MAXPOSTS 5
#define DELTA 1 // units of milliseconds.

// These are individual posts, all posts consist of a photo and a caption.
struct topicEntry {
    int entryNum;
    struct timeval timeStamp;
    int pubID;
    char photoURL[URLSIZE];
    char photoCaption[CAPSIZE];
};

// This is the circular queue buffer our publishers post to and our subscribers read from.
typedef struct topicQueue {
    char name[MAXNAME];
    pthread_mutex_t mutex;
    struct topicEntry buffer[MAXPOSTS];
    int totalPastPosts; // This is an easy way to give each post in the topic a unique ID.
    int head;
    int tail;
    int totalCapacity; // The size of the buffer, i.e MAXPOSTS.
    int bufferEntries;
} topicQueue;

/*
 * This is the entirety of the server's store.
 */
topicQueue topicStore[MAXTOPICS];

// Retrieves queue named topicID from the registry of topics. Program terminates on failure.
topicQueue *getQueue(const char *topicID) {
    for (int i = 0; i < MAXTOPICS; i++) {
        if (!strcmp(topicID, topicStore[i].name)) {
            return &topicStore[i];
        }
    }
    fprintf(stderr, "Attempt to access topicQueue that does not exist :%s:\n", topicID);
    exit(EXIT_FAILURE);
}

// Prints a topic entry.
void viewPost(struct topicEntry post) {
    printf("entryNum: %d, photoURL %s, photoCaption %s\n", post.entryNum, post.photoURL, post.photoCaption);
}

// Prints a topic queue.
void viewQueue(char *topicID) {
    topicQueue *topic = getQueue(topicID);
    printf("name: %s, totalPastPosts: %d, head: %d, tail %d, totalCapcity %d, bufferEntries %d",
           topic->name, topic->totalPastPosts, topic->head, topic->tail, topic->totalCapacity, topic->bufferEntries);
}

/*
 * This function will attempt to enqueue the topic entry TE into the topic queue with name TopicID. Note that this
 * function contains a critical section and will block until the topic queue with name TopicID's mutex is unlocked.
*/
void enqueue(char *topicID, struct topicEntry post) {
    topicQueue *topic = getQueue(topicID);
    pthread_mutex_lock(&topic->mutex);
    while (topic->totalCapacity == topic->bufferEntries) {
        // TODO: Change to a semaphore instead of looping.
        printf("here!!!!\n");
        pthread_mutex_unlock(&topic->mutex);
        sched_yield();
        pthread_mutex_lock(&topic->mutex);
    }
    ++topic->totalPastPosts;
    gettimeofday(&post.timeStamp, NULL);
    post.entryNum, post.pubID = topic->totalPastPosts, topic->totalPastPosts;
    topic->buffer[topic->head] = post;
    topic->head = (topic->head + 1) % topic->totalCapacity;
    ++topic->bufferEntries;
    pthread_mutex_unlock(&topic->mutex);
}

/*
 * This function will attempt to dequeue a post from the topic queue named topicID if the oldest post is older than
 * DELTA. Note that this function contains a critical section and will block until the topic queue with name topicID's
 * mutex is unlocked.
*/
void dequeue(char *topicID) {
    topicQueue *topic = getQueue(topicID);
    pthread_mutex_lock(&topic->mutex);
    if (!topic->bufferEntries) {
        pthread_mutex_unlock(&topic->mutex);
        return;
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
}

/*
 * This routine will take three arguments: A) An integer argument lastEntry which is the number of the last entry read
 * by the calling thread on this topic, B) A reference to an empty topicEntry struct, and C) a topicID. The routine will
 * attempt to get the lastEntry+1 entry if it is in the topic queue, or the oldest entry younger than lastEntry+1 if
 * not.
*/
int getEntry(int *lastEntry, struct topicEntry *post, char *topicID) {
    int newEntry = *lastEntry + 1;
    viewQueue(topicID);
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


int main() {
    printf("Hello, World!\n");
    strcpy(topicStore[0].name, "apples");
    topicStore[0].tail, topicStore[0].head, topicStore[0].totalPastPosts = 0, 0, 0;
    pthread_mutex_init(&topicStore[0].mutex, NULL);
    topicStore[0].totalCapacity = 5;

    for (int i = 0; i < 5; i++) {
        enqueue("apples", *(struct topicEntry*)malloc(sizeof(struct topicEntry)));
        dequeue("apples");
        sleep(1);
    }
    struct topicEntry placeholder;
    int a = 0;
    for (int i = 0; i < 10; i++) {
        getEntry(&a, &placeholder, "apples");
        printf("a! %d\n", a);
        viewPost(placeholder);
    }
    return 0;
}
