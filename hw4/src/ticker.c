#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include "ticker.h"

typedef struct watcher{
    int enable;
} WATCHER;

int msg_ready(int sig){
    return 0;
}

void handler(int sig);

int ticker(void) {
    // TO BE IMPLEMENTED

    struct sigaction CLI = {0};
    CLI.sa_handler = handler;
    sigaction(SIGINT, &CLI, NULL);
    char input[100];
    while(1){ // as long as the user did not quit the program keep listening for input
        *input = 0;
        printf("ticker> ");
        fgets(input,100,stdin);
        printf("%s",input);
        sigsuspend(&(CLI.sa_mask));
        printf("resume\n");
    }

    //reap all the remaining watchers

    //kill the CLI watcher as well
}


void handler(int sig){
    printf("%d\n",sig);
}