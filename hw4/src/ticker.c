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

typedef struct link_list{
    int index;
    pid_t pid;
    WATCHER *watcher;
    struct link_list *next;
} TABLE;

void terminated(int sig, siginfo_t *info, void * ucontext);
void msg_ready(int sig, siginfo_t *info, void * ucontext);
void fctnl_setup(int fd);
void evaluate(char input[],size_t size);
char **parse(char input[]);

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

    fctnl_setup(STDIN_FILENO);

    char input[128];
    while(1){ // as long as the user did not quit the program keep listening for input

        printf("ticker> ");
        fflush(stdout);
        // Waits for user input with current process suspended
        sigsuspend(&set);
        memset(input, 0, sizeof(input)); // Clear the buffer
        if(read(STDIN_FILENO,input,sizeof(input)) < 0){
            perror("reading from user input failed\n");
        } // read whatever
        if(errno == EWOULDBLOCK) // if nothing continue
            continue;
        evaluate(input, sizeof(input));
    }
    return 0;
}

/*
    signal handler for notification of a message from a watcher process
*/
void msg_ready(int sig, siginfo_t *info, void * ucontext){
}

/*
    signal handler for termination of a watcher proces
*/
void terminated(int sig, siginfo_t *info, void * ucontext){
    //reap the terminated child
}


void fctnl_setup(int fd){
    if(fcntl(fd,F_SETFL, fcntl(0, F_GETFL) |O_ASYNC|O_NONBLOCK) < 0){
        perror("Setting STDIN to async failed");
    }
    if(fcntl(fd,F_SETOWN,getpid()) < 0){
        perror("Setting receving process of SIGIO signal failed");
    }
    if(fcntl(fd,F_SETSIG,SIGIO) < 0){
        perror("Setting signal handler for async STDIN failed.");
    }
}

void evaluate(char input[], size_t size){
    char *str  = strtok(input, " ");
    if(strcmp(str, "start") == 0){
        printf("start\n");
        str = strtok(NULL,"\n");
        printf("%s\n",str);
    }else if(strcmp(str, "watchers\n") == 0){
        printf("watchers\n");
    }else if(strcmp(str, "stop") == 0){
        printf("stop\n");
        str = strtok(NULL,"\n");
        printf("%s\n",str);
    }else if(strcmp(str, "show") == 0){
        printf("show\n");
        str = strtok(NULL,"\n");
        printf("%s\n",str);
    }else if(strcmp(str, "trace") == 0){
        printf("trace\n");
        str = strtok(NULL,"\n");
        printf("%s\n",str);
    }else if(strcmp(str,"untrace") == 0){
        printf("untrace\n");
        str = strtok(NULL,"\n");
        printf("%s\n",str);
    }else if(strcmp(str, "quit\n") == 0){
        printf("quit\n");
    }else{
        printf("???\n");
    }
}