#include "ticker.h"
#include <unistd.h>


struct watcher{
    pid_t pid;
    int enable; // if the watcher is currently being traced
    int inputfd;
    int outputfd;
    char *name;
};

typedef struct link_list{
    int index;
    WATCHER *watcher;
    struct link_list *next;
} NODE;

