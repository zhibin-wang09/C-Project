#include <stdlib.h>

#include "ticker.h"
#include "store.h"
#include "cli.h"
#include "debug.h"
#include "watcher_define.h"


WATCHER *cli_watcher_start(WATCHER_TYPE *type, char *args[]) {
    // TO BE IMPLEMENTED
    WATCHER  *cli_watcher = malloc(sizeof(WATCHER));
    if(cli_watcher == NULL){
        perror("Ran out of memory");
    }
    cli_watcher -> pid = -1;
    cli_watcher -> enable = 0;
    cli_watcher -> parent_inputfd = STDIN_FILENO;
    cli_watcher -> parent_outputfd = STDOUT_FILENO;
    cli_watcher -> name = type -> name;
    cli_watcher -> child_inputfd = -1;
    cli_watcher -> child_outputfd = -1;
    return cli_watcher;
}

int cli_watcher_stop(WATCHER *wp) {
    // TO BE IMPLEMENTED
    return -1;
}

int cli_watcher_send(WATCHER *wp, void *arg) {
    // TO BE IMPLEMENTED
    abort();
}

int cli_watcher_recv(WATCHER *wp, char *txt) {
    // TO BE IMPLEMENTED
    abort();
}

int cli_watcher_trace(WATCHER *wp, int enable) {
    // TO BE IMPLEMENTED
    abort();
}
