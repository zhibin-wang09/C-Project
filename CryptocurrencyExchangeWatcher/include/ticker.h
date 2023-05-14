/*
 * DO NOT MODIFY THE CONTENTS OF THIS FILE.
 * IT WILL BE REPLACED DURING GRADING
 */
#ifndef TICKER_H
#define TICKER_H

/*
 * Type of an object that stores the state of a "watcher".
 * You have to decide how you want to define "struct watcher".
 */
typedef struct watcher WATCHER;

/*
 * Methods for manipulating watchers.
 * See the assignment handout for further details.
 */
typedef struct watcher_type {
    char *name;
    char **argv;
    struct watcher *(*start)(struct watcher_type *type, char *args[]);
    int (*stop)(WATCHER *wp);
    int (*send)(WATCHER *wp, void *msg);
    int (*recv)(WATCHER *wp, char *txt);
    int (*trace)(WATCHER *wp, int enable);
} WATCHER_TYPE;

// Watcher types

#define CLI_WATCHER_TYPE 0
#define BITSTAMP_WATCHER_TYPE 1
#define BLOCKCHAIN_WATCHER_TYPE 2

extern WATCHER_TYPE watcher_types[];

// Entry point for "ticker", called from main() after argument processing.
int ticker(void);

#endif
