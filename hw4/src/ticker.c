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

int ticker(void) {
    sigset_t set;
    sigfillset(&set);
    sigdelset(&set,SIGIO);
    sigdelset(&set,SIGCHLD);
    sigdelset(&set,SIGINT);
    sigdelset(&set,SIGQUIT);
    sigdelset(&set,SIGTERM);
    sigdelset(&set,SIGSTOP);
    struct sigaction termination = {0}; // the signal for notifying that a watcher process has terminated
    termination.sa_sigaction = terminated;
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
    int cur_length = 128;
    int bytes_read = 0;
    char *input = malloc(cur_length); //calloc(cur_length,1); // need to be dynamically resized
    int ret = 0;
    int ticker_print = 1;
    while(1){ // as long as the user did not quit the program keep listening for input
        if(ticker_print == 1){
            printf("ticker> ");
            fflush(stdout);
        }
        // Waits for user input with current process suspended
        memset(input,0,cur_length);
        while((ret = read(STDIN_FILENO,input+bytes_read,cur_length)) > 0){
            bytes_read += ret;
            if(bytes_read >= cur_length){
                input = realloc(input,cur_length*2);
                cur_length *=2;
            }
        }
        //printf("ret: %d, errno: %d,bytesread: %d,cur_length: %d, input: %s\n",ret,errno,bytes_read,cur_length,input);
        if(ret == -1 && errno != EWOULDBLOCK) perror("Reading input failed");
        if(errno == EWOULDBLOCK) { // in the case where read has read everything or there is not input
            if(bytes_read != 0){ // input is ready and stored in buffer
                evaluate(input, sizeof(input));
                ticker_print = 1;
                bytes_read =0;
                continue;
            }else{ // no input read
                ticker_print =0;
            }
            sigsuspend(&set); // stop until input is read
            bytes_read = 0;
            continue;
        }
        if(ret == 0){ // ret is 0 means input from echo
            evaluate(input, sizeof(input));
            ticker_print = 1;
            bytes_read = 0;
        }
    }
    return 0;
}
/*
    signal handler for notification of a message from a watcher process
*/
void msg_ready(int sig, siginfo_t *info, void * ucontext){
    //write(STDIN_FILENO, "msg ready\n",10);
}

/*
    signal handler for termination of a watcher proces
*/
void terminated(int sig, siginfo_t *info, void * ucontext){
    //reap the terminated child
}


void fctnl_setup(int fd){
    if(fcntl(fd,F_SETFL, O_ASYNC|O_NONBLOCK) < 0){
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
    if(strlen(input) == 0) exit(-1);
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
