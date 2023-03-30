/*
 * DO NOT MODIFY THE CONTENTS OF THIS FILE.
 * IT WILL BE REPLACED DURING GRADING
 */

WATCHER *bitstamp_watcher_start(WATCHER_TYPE *type, char *args[]);
int bitstamp_watcher_stop(WATCHER *wp);
int bitstamp_watcher_send(WATCHER *wp, void *msg);
int bitstamp_watcher_recv(WATCHER *wp, char *txt);
int bitstamp_watcher_trace(WATCHER *wp, int enable);
