#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "ticker.h"

typedef struct watcher{
    pid_t pid;
    int enable;
    int inputfd;
    int outputfd;
    char *name;
} WATCHER;

typedef struct{
    pid_t pid;
    WATCHER watcher;
    int index;
} link_table;

void terminated(int sig, siginfo_t *info, void * ucontext);
void msg_ready(int sig, siginfo_t *info, void * ucontext);
void fctnl_setup();

int ticker(void) {
    sigset_t set;
    sigfillset(&set);
    sigdelset(&set,SIGQUIT);
    sigdelset(&set,SIGINT);
    sigdelset(&set,SIGCHLD);
    sigdelset(&set,SIGIO);
    struct sigaction termination = {0}; // the signal for notifying that a watcher process has terminated
    termination.sa_sigaction = terminated;
    termination.sa_flags = SA_SIGINFO;
    sigemptyset(&termination.sa_mask);
    if(sigaction(SIGCHLD, &termination, NULL) < 0){
        perror("Creating SIGCHILD failed");
        return -1;
    }


    struct sigaction notification = {0}; // the signal for notifying that a message from watcher is ready
    notification.sa_flags = SA_SIGINFO;
    notification.sa_sigaction = &msg_ready;
    sigemptyset(&notification.sa_mask);
    if(sigaction(SIGIO, &notification, NULL) < 0){
        perror("Creating SIGIO failed");
        return -1;
    }

    fctnl_setup();

    char input[100];
    int c;
    while(1){ // as long as the user did not quit the program keep listening for input
        *input = 0;

        printf("ticker> ");
        fflush(stdout);
        // Waits for user input with current process suspended
        sigsuspend(&set);
        read(STDIN_FILENO,input,100); // read whatever
        if(errno == EWOULDBLOCK) // if nothing continue
            continue;
        FILE *inputf =fmemopen(input,strlen(input),"r");
        while((c = fgetc(inputf)) != EOF){
            
        } // read the input char by char

    }
}

/*
    signal handler for notification of a message from a watcher process
*/
void msg_ready(int sig, siginfo_t *info, void * ucontext){
    printf("SIGIO FIRED\n");
}

/*
    signal handler for termination of a watcher proces
*/
void terminated(int sig, siginfo_t *info, void * ucontext){
    //reap the terminated child
}


void fctnl_setup(){
    if(fcntl(STDIN_FILENO,F_SETFL, fcntl(0, F_GETFL) |O_ASYNC|O_NONBLOCK) < 0){
        perror("Setting STDIN to async failed");
    }
    if(fcntl(STDIN_FILENO,F_SETOWN,getpid()) < 0){
        perror("Setting receving process of SIGIO signal failed");
    }
    if(fcntl(STDIN_FILENO,F_SETSIG,SIGIO) < 0){
        perror("Setting signal handler for async STDIN failed.");
    }
}