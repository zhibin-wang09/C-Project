/*
 * DO NOT MODIFY THE CONTENTS OF THIS FILE.
 * IT WILL BE REPLACED DURING GRADING
 */

#include <stdio.h>

#include "ticker.h"
#include "cli.h"
#include "bitstamp.h"

WATCHER_TYPE watcher_types[] = {
    [CLI_WATCHER_TYPE]
    {.name = "CLI",
     .argv = NULL,
     .start = cli_watcher_start,
     .stop = cli_watcher_stop,
     .send = cli_watcher_send,
     .recv = cli_watcher_recv,
     .trace = cli_watcher_trace},

    [BITSTAMP_WATCHER_TYPE]
    {.name = "bitstamp.net",
     .argv = (char *[]){"uwsc", "wss://ws.bitstamp.net", NULL},
     .start = bitstamp_watcher_start,
     .stop = bitstamp_watcher_stop,
     .send = bitstamp_watcher_send,
     .recv = bitstamp_watcher_recv,
     .trace = bitstamp_watcher_trace},

    {0}
};

