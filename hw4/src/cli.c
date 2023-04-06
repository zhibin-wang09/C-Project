#include <stdlib.h>

#include "ticker.h"
#include "lib/store.h"
#include "cli.h"
#include "debug.h"

extern int add_to_table(WATCHER *watcher);


WATCHER *cli_watcher_start(WATCHER_TYPE *type, char *args[]) {
    // TO BE IMPLEMENTED
    WATCHER cli_watcher = {-1,0,1,2,"CLI"};
    add_to_table(&cli_watcher);
    return &cli_watcher;
}

int cli_watcher_stop(WATCHER *wp) {
    // TO BE IMPLEMENTED
    abort();
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
