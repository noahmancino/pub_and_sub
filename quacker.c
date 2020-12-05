/*
 * Describe the project.
 */
#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>
#include <string.h>
#include <bits/sigthread.h>
#include "project3.h"

/*
 * This function will attempt to enqueue the topic entry TE into the topic queue with id TopicID. Note that this
 * function contains a critical section and will block until the topic queue with name TopicID's mutex is unlocked.
*/
int enqueue(int topicID, struct topicEntry post) {
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
    viewQueue(topic);
    pthread_mutex_unlock(&topic->mutex);
    return EXIT_SUCCESS;
}


/*
 * This is the start routine for a publisher thread. It takes an array of arguments to enqueue and calls enqueue on them.
 */
void *publisher(void *arg) {
    signal(SIGCONT, catch);
    fflush(stdout);
    pause();
    char *filename = (char *) arg;
    FILE *commandFile = fopen(filename, "r");
    char *line = NULL;
    size_t n = 0;
    for (int i = 0; getline(&line, &n, commandFile) != -1; i++) {
        command_line apple = str_filler(line, " ");
        char **lineTokens = apple.command_list;
        if (!strcmp(lineTokens[0], "sleep")) {
            //printf("Executed command: sleep\n");
            usleep(atoi(lineTokens[1]));
        } else if (!strcmp(lineTokens[0], "stop")) {
            free_command_line(&apple);
            free(line);
            return EXIT_SUCCESS;
        } else if (!strcmp(lineTokens[0], "put")) {
            int topicID = atoi(lineTokens[1]);
            char *URL = removeQuotes(lineTokens[2]);
            struct topicEntry newPost = newTopicEntry(URL, "");
            for (int k = 3; k < apple.num_token - 1; k++) {
                strcat(newPost.photoCaption, lineTokens[k]);
                strcat(newPost.photoCaption, " ");
            }
            enqueue(topicID, newPost);
        }
        free_command_line(&apple);
    }
    free(line);
}

/*
 * This function will attempt to dequeue a post from the topic queue with id topicID if the oldest post is older than
 * delta. Note that this function contains a critical section and will block until the topic queue with id topicID's
 * mutex is unlocked.
*/
void *dequeue(void *topicID) {
    topicQueue *topic = getQueue(*(int *) topicID);
    pthread_mutex_lock(&topic->mutex);
    if (!topic->bufferEntries) {
        pthread_mutex_unlock(&topic->mutex);
        return (void *) EXIT_FAILURE;
    }
    struct topicEntry dequeued = topic->buffer[topic->tail];
    struct timeval curDelta;
    gettimeofday(&curDelta, NULL);
    curDelta.tv_sec -= dequeued.timeStamp.tv_sec;
    if (curDelta.tv_sec >= delta) {
        topic->tail = (topic->tail + 1) % topic->totalCapacity;
        --topic->bufferEntries;
    }
    pthread_mutex_unlock(&topic->mutex);
    return EXIT_SUCCESS;
}

/*
 * The start routine for the cleaner thread. Attempts to dequeue from each topic each time delta milliseconds have
 * elapsed.
 */
void *cleaner(void *arg) {
    int topics = *(int *) arg;
    while (1) {
        for (int i = 0; i < topics; i++) {
            dequeue(&topicStore[i].id);
        }
        sleep(delta);
    }
    return EXIT_SUCCESS;
}

/*
 * This routine takes three arguments: A) A pointer to an int, lastEntry, which is the number of the last entry read by
 * the calling thread on this topic, B) A reference to an empty topicEntry struct, and C) a topicID. The routine will
 * attempt to get the lastEntry+1 entry if it is in the topic queue, or the oldest entry younger than lastEntry+1 if
 * not.
*/
int getEntry(int *lastEntry, struct topicEntry *post, int topicID) {
    int newEntry = *lastEntry + 1;
    topicQueue *topic = getQueue(topicID);
    if (!topic->bufferEntries) {
        return EXIT_FAILURE;
    }
    pthread_mutex_lock(&topic->mutex);
    // If the next entry isn't in the queue, we need to know if there is a younger/later entry or not.
    int was_dequed = 0;
    for (int i = 0; i < topic->bufferEntries; i++) {
        struct topicEntry entry = topic->buffer[(topic->tail + i) % topic->totalCapacity];
        if (entry.entryNum == newEntry) {
            // Case where newEntry is in the queue
            pthread_mutex_unlock(&topic->mutex);
            memcpy(post, &entry, sizeof(entry));
            *lastEntry = newEntry;
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
        memcpy(post, &oldest, sizeof(oldest));
        *lastEntry = oldest.entryNum;
        return EXIT_SUCCESS;
    }
    // Case where neither newEntry or anything younger than newEntry has been placed in the queue.
    pthread_mutex_unlock(&topic->mutex);
    fflush(stdout);
    return EXIT_FAILURE;
}

/*
 * Start routine for the subscriber thread. Takes an array of getEntry args and the length of the array, calls
 * getEntry for each one, and displays the ticket it fills.
 */
void *subscriber(void *arg) {
    signal(SIGCONT, catch);
    pause();
    int lastEntry = 0;
    struct topicEntry buffer;
    char *filename = (char *) arg;
    char writefilename[50];
    sprintf(writefilename, "SUB:%lu.txt", pthread_self());
    FILE *writefile = fopen(writefilename, "w");
    FILE *commandFile = fopen(filename, "r");
    char *line = NULL;
    size_t n = 0;
    for (int i = 0; getline(&line, &n, commandFile) != -1; i++) {
        command_line bananas = str_filler(line, " ");
        char **lineTokens = bananas.command_list;
        if (!strcmp(lineTokens[0], "sleep")) {
            //     printf("Proxy thread %lu - type: subscriber - Executed command: sleep\n", pthread_self());
            usleep(atoi(lineTokens[1]));
        } else if (!strcmp(lineTokens[0], "stop")) {
            //        printf("Proxy thread %lu - type: subscriber - Executed command: stop\n", pthread_self());
            free(line);
            free_command_line(&bananas);
            fclose(writefile);
            fclose(commandFile);
            return EXIT_SUCCESS;
        } else if (!strcmp(lineTokens[0], "get")) {
            //     printf("Proxy thread %lu - type: subscriber - Executed command: get\n", pthread_self());
            if (!getEntry(&lastEntry, &buffer, atoi(lineTokens[1]))) {
                topicQueue *topic = getQueue(atoi(lineTokens[1]));
                fprintf(writefile, "Topic: %s\n", topic->name);
                viewPost(writefile, buffer);
            }
        }
        free_command_line(&bananas);
    }
}

int main(int argc, char *argv[]) {
    signal(SIGSEGV, segHandler);
    signal(SIGUSR1, leaveThread);
    // Keep track of jobs when thread pool is full.
    int numOverflowPubs = 0;
    int numOverflowSubs = 0;
    char overflowPubs[MAXCOMMANDS][MAXNAME];
    char overflowSubs[MAXCOMMANDS][MAXNAME];
    overflowPubs[0][0] = '\0';
    overflowSubs[0][0] = '\0';
    char *masterCommandFile = argv[1];
    char ***tokenizedMaster = tokenize(masterCommandFile);
    stores = 0;
    for (int i = 0; tokenizedMaster[i] != NULL; i++) {
        // Create a topic with ID (integer) and length. This allocates a topic queue.
        char **lineTokens = tokenizedMaster[i];
        if (!strcmp(lineTokens[0], "create")) {
            int topicID = atoi(lineTokens[2]);
            char *name = removeQuotes(lineTokens[3]);
            int bufferSize = atoi(lineTokens[4]);
            topicStore[stores] = newTopicQueue(name, topicID, bufferSize);
            ++stores;
            // TODO: free topic buffers
            // TODO: make sure things don't break because the buffer is mallocd.
        }
            // Start all of the publishers and subscribers, as well as the cleanup thread.
        else if (!strcmp(lineTokens[0], "start")) {
            pthread_create(&clean.thread, NULL, cleaner, (void *) &stores);
            // Might not get setup before getting SIGCONT if you don't wait.
            usleep(150);
            for (int k = 0; k < NUMPROXIES / 2; k++) {
                if (publisherPool[k].isNotFree) {
                    pthread_kill(publisherPool[k].thread, SIGCONT);
                }
                if (subscriberPool[k].isNotFree) {
                    pthread_kill(subscriberPool[k].thread, SIGCONT);
                }
            }
            for (int k = 0; k < NUMPROXIES / 2; k++) {
                if (publisherPool[k].isNotFree) {
                    pthread_join(publisherPool[k].thread, NULL);
                    publisherPool[k].isNotFree = 0;
                }
                if (subscriberPool[k].isNotFree) {
                    pthread_join(subscriberPool[k].thread, NULL);
                    subscriberPool[k].isNotFree = 0;
                }
            }
        }
            // Set delta (determines how long until posts get cleaned from store) to specified value.
        else if (!strcmp(lineTokens[0], "delta")) {
            delta = atoi(lineTokens[1]);
        }
            /* Adds a job to the publisher threads workload. A free thread is allocated to be the “proxy" for the
             * publisher
             */
        else if (!strcmp(lineTokens[1], "publisher")) {
            char *filename = removeQuotes(lineTokens[2]);
            for (int k = 0; k < NUMPROXIES / 2; k++) {
                if (!publisherPool[k].isNotFree) {
                    publisherPool[k].isNotFree = 1;
                    pthread_create(&publisherPool[k].thread, NULL, publisher, filename);
                    break;
                }
            }
            // TODO: deal with no available threads.
        }
            /* Adds a job to the subscriber threads workload A free thread is allocated to be the “proxy" for the
             * subscriber
             */
        else if (!strcmp(lineTokens[1], "subscriber")) {
            char *filename = removeQuotes(lineTokens[2]);
            int k;
            for (k = 0; k < NUMPROXIES / 2; k++) {
                if (!subscriberPool[k].isNotFree) {
                    subscriberPool[k].isNotFree = 1;
                    pthread_create(&subscriberPool[k].thread, NULL, subscriber, filename);
                    break;
                }
            }
        }
        // Prints topic queues to console
        else if (!strcmp(lineTokens[0], "query")) {
            for (int k = 0; k < stores; k++) {
                viewQueue(&topicStore[k]);
            }
        }

    }
    pthread_kill(clean.thread, SIGUSR1);
    pthread_join(clean.thread, NULL);
    freeTokens(tokenizedMaster);
    return 0;
}
