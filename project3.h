//
// Created by noahmancino on 12/2/20.
//
#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <zconf.h>
#include <signal.h>

#ifndef PROJECT3_PROJECT3_H
#define PROJECT3_PROJECT3_H
#define MAXTOPICS 4
#define URLSIZE 100
#define CAPSIZE 200
#define MAXNAME 25
#define MAXPOSTS 100
#define NUMPROXIES 10
#define MAXTOKENS 10
#define MAXCOMMANDS 100
#define MAXTOKEN 100

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
    struct topicEntry *buffer;
    int id;
    int totalPastPosts; // This is an easy way to give each post in the topic a unique ID.
    int head;
    int tail;
    int totalCapacity; // The size of the buffer, i.e MAXPOSTS.
    int bufferEntries;
} topicQueue;

struct enqueueArgs {
    char *topicID;
    struct topicEntry post;
};

struct getEntryArgs {
    char *topicID;
    struct topicEntry *post;
    int *lastEntry;
};

struct threadPoolMember {
    short isNotFree;
    pthread_t thread;
};

// This is the entirety of the servers store.
topicQueue topicStore[MAXTOPICS];
struct threadPoolMember publisherPool[NUMPROXIES / 2];
struct threadPoolMember subscriberPool[NUMPROXIES / 2];
struct threadPoolMember clean;
int delta = 1;


/*
 * Checks thread pool array (note that it assumes it is of length NUMPROXIES / 2) for free threads. If one exists, it
 * returns a pointer to it and marks the thread unavailable. Otherwise, it returns the null pointer.
 */
pthread_t *getFreeThread(struct threadPoolMember *threadPool) {
    for (int i = 0; i < NUMPROXIES / 2; i++) {
        if (!threadPool[i].isNotFree) {
            threadPool[i].isNotFree = 1;
            return &threadPool[i].thread;
        }
    }
    return NULL;
}

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

topicQueue newTopicQueue(char *name, int topicID, int bufferSize) {
    topicQueue new;
    new.buffer = (struct topicEntry *)malloc(sizeof(struct topicEntry) * bufferSize);
    strcpy(new.name, name);
    new.tail = 0, new.head = 0, new.totalPastPosts = 0, new.bufferEntries = 0, new.id = topicID;
    pthread_mutex_init(&new.mutex, NULL);
    new.totalCapacity = bufferSize;
    return new;
}

struct topicEntry newTopicEntry(char *URL, char *caption) {
    struct topicEntry new;
    strcpy(new.photoCaption, caption);
    strcpy(new.photoURL, URL);
    return new;
}

/*
 * Given the name of a text file this function returns an array of arrays of tokens in the files' text. The outer
 * arrays are delimited by newlines, the inner arrays are delimited by whitespace.
 */
char ***tokenize(const char *filename) {
    char ***parsedLines = (char ***)malloc(sizeof(char **) * MAXCOMMANDS);
    FILE *commandFile = fopen(filename, "r");
    char *line = NULL;
    size_t n = 0;
    int i;
    for (i = 0; getline(&line, &n, commandFile) != -1; i++) {
        parsedLines[i] = (char **)malloc(sizeof(char *) * MAXTOKENS);
        char *token = strtok(line, " \n");
        int j;
        for (j = 0; token != NULL; j++) {
            parsedLines[i][j] = (char *)malloc(sizeof(char) * MAXTOKEN);
            strcpy(parsedLines[i][j], token);
            token = strtok(NULL, " \n");
        }
        parsedLines[i][j] = NULL;

    }
    free(line);
    fclose(commandFile);
    parsedLines[i] = NULL;
    return parsedLines;
}

/*
 * Frees a 3-d array where the array is mallocd at each level.
 */
void freeTokens(char ***tokenized) {
    for (int i = 0; tokenized[i] != NULL; i++) {
        for (int j = 0; tokenized[i][j] != NULL; j++) {
            free(tokenized[i][j]);
        }
        free(tokenized[i]);
    }
    free(tokenized);
}


#endif //PROJECT3_PROJECT3_H
