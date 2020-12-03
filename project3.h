//
// Created by noahmancino on 12/2/20.
//
#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <zconf.h>


#ifndef PROJECT3_PROJECT3_H
#define PROJECT3_PROJECT3_H
#define MAXTOPICS 4
#define URLSIZE 100
#define CAPSIZE 200
#define MAXNAME 25
#define MAXPOSTS 5
#define DELTA 100
#define NUMPROXIES 10

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

struct publisherArgs {
    char *topicID;
    struct topicEntry post;
};

struct subscriberArgs {
    char *topicID;
    struct topicEntry *post;
    int *lastEntry;
};

struct threadPoolMember {
    short i; // Flag indicating whether a thread is free.
    pthread_t thread;
};

// This is the entirety of the servers store.
topicQueue topicStore[MAXTOPICS];
pthread_t publisher_threads[NUMPROXIES/2];
pthread_t subscriber_threads[NUMPROXIES/2];
pthread_t cleaner;


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
void viewQueue(topicQueue *topic) {
    printf("name: %s, totalPastPosts: %d, head: %d, tail %d, totalCapcity %d, bufferEntries %d",
           topic->name, topic->totalPastPosts, topic->head, topic->tail, topic->totalCapacity, topic->bufferEntries);
}

topicQueue newTopicQueue(char *name) {
    topicQueue new;
    strcpy(new.name, name);
    new.tail = 0, new.head = 0, new.totalPastPosts = 0, new.bufferEntries = 0;
    pthread_mutex_init(&new.mutex, NULL);
    new.totalCapacity = MAXPOSTS;
    return new;
}

struct topicEntry newTopicEntry(char *URL, char *caption) {
    struct topicEntry new;
    strcpy(new.photoCaption, caption);
    strcpy(new.photoURL, URL);
    return new;
}


#endif //PROJECT3_PROJECT3_H
