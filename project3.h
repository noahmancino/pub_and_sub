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
#define URLSIZE 150
#define CAPSIZE 200
#define MAXNAME 25
#define MAXPOSTS 100
#define NUMPROXIES 10
#define MAXTOKENS 15
#define MAXCOMMANDS 50
#define MAXTOKEN 200

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
int stores;

static void catch(int signal) { }

static void iLikeFruit(int signal) {
    printf("it was me!!! %lu\n\n\n\n\n\n\n\n\n\n\n\n", pthread_self());
    pthread_exit(NULL);
}

static void pudding(int signal) {
    pthread_exit(NULL);
}

// Retrieves queue named topicID from the registry of topics. Program terminates on failure.
topicQueue *getQueue(int topicID) {
    for (int i = 0; i < stores; i++) {
        if (topicStore[i].id == topicID) {
            return &topicStore[i];
        }
    }
    fprintf(stderr, "Attempt to access topicQueue that does not exist :%d:\n", topicID);
    exit(EXIT_FAILURE);
}

// Prints a topic entry.
void viewPost(FILE *file, struct topicEntry post) {
    fprintf(file, "entryNum: %d, photoURL %s, photoCaption %s\n", post.entryNum, post.photoURL, post.photoCaption);
}

// Prints a topic queue.
void viewQueue(topicQueue *topic) {
    printf("name: %s, totalPastPosts: %d, totalCapcity %d, bufferEntries %d pid: %d\n",
           topic->name, topic->totalPastPosts, topic->totalCapacity, topic->bufferEntries, topic->id);
}

// init for topic queues
topicQueue newTopicQueue(char *name, int topicID, int bufferSize) {
    topicQueue new;
    new.buffer = (struct topicEntry *)malloc(sizeof(struct topicEntry) * bufferSize);
    strcpy(new.name, name);
    new.tail = 0, new.head = 0, new.totalPastPosts = 0, new.bufferEntries = 0, new.id = topicID;
    pthread_mutex_init(&new.mutex, NULL);
    new.totalCapacity = bufferSize;
    return new;
}

// Init for topic entries
struct topicEntry newTopicEntry(char *URL, char *caption) {
    struct topicEntry new;
    strcpy(new.photoCaption, caption);
    strcpy(new.photoURL, URL);
    return new;
}

// Given a string encased in quotes, returns the string with the quotes removed.
char *removeQuotes(char *string) {
    ++string;
    fflush(stdout);
    if (strlen(string)) string[strlen(string)-1] = '\0';
    return string;
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

void readToks(char ***toks) {
    for (int i = 0; toks[i] != NULL; i++) {
        for (int j = 0; toks[i][j] != NULL; j++) {
            printf("%s ", toks[i][j]);
        }
        printf("\n");
    }
    fflush(stdout);
    printf("\n");
}

int count_token (char* buf, const char* delim) {
    if (buf == NULL) {
        printf ("Empty string\n");
        return -1;
    }
    int len = strlen (buf);
    int n_token = 0;
    for (int i = 0; i < len; i++) {
        if (i == 0 && buf[i] == delim[0]) {
            continue;
        }
        if (buf[i] == delim[0])
        {
            n_token++;
            if (i == len - 1) {
                return n_token + 1;
            }
        }
    }
    n_token = n_token + 2;
    return n_token;
}

typedef struct {
    char** command_list;
    int num_token;
} command_line;

command_line str_filler (char* buf, const char* delim)
{
    command_line command;

    int num_token;
    char* token = NULL;
    char* savePtr = NULL;

    token = strtok_r (buf, "\n", &savePtr);
    num_token = count_token (buf, delim);

    command.command_list = malloc (sizeof(char*) * num_token);
    command.num_token = num_token;
    token = strtok_r (token, delim, &savePtr);


    for (int i = 0; i < num_token - 1; i++)
    {

        command.command_list[i] = malloc (strlen (token) + 1);

        strcpy (command.command_list[i], token);

        token = strtok_r (savePtr, delim, &savePtr);
    }

    command.command_list[num_token - 1] = NULL;

    return command;
}


void free_command_line(command_line* command)
{
    for (int i = 0; i < command->num_token; i++)
    {
        free (command->command_list[i]);
    }
    free (command->command_list);
}

#endif //PROJECT3_PROJECT3_H
