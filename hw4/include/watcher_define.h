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
    char *args;
    int index;
    int serial_num;
};

typedef struct link_list{
    int index;
    WATCHER *watcher;
    struct link_list *next;
} NODE;

void add_to_table(WATCHER *watcher);
void print_table();
WATCHER *search_table(int index);
void remove_from_table(int index);
