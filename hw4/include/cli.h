/*
 * DO NOT MODIFY THE CONTENTS OF THIS FILE.
 * IT WILL BE REPLACED DURING GRADING
 */

WATCHER *cli_watcher_start(WATCHER_TYPE *type, char *args[]);
int cli_watcher_stop(WATCHER *wp);
int cli_watcher_send(WATCHER *wp, void *msg);
int cli_watcher_recv(WATCHER *wp, char *txt);
int cli_watcher_trace(WATCHER *wp, int enable);
