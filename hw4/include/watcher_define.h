#include "ticker.h"
#include <unistd.h>


struct watcher{
    pid_t pid;
    int enable; // if the watcher is currently being traced
    int parent_inputfd;
    int parent_outputfd;
    int child_inputfd;
    int child_outputfd;
    char *name;
};

typedef struct link_list{
    int index;
    WATCHER *watcher;
    struct link_list *next;
} NODE;

