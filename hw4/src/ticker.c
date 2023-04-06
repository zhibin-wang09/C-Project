#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "ticker.h"

struct watcher{
    pid_t pid;
    int enable; // if the watcher is currently being traced
    int inputfd;
    int outputfd;
    char *name;
};

typedef struct link_list{
    int index;
    pid_t pid;
    WATCHER *watcher;
    struct link_list *next;
} TABLE;



void terminated(int sig, siginfo_t *info, void * ucontext);
void msg_ready(int sig, siginfo_t *info, void * ucontext);
void fctnl_setup(int fd);
void evaluate(char input[],int *stop);
void add_to_table(WATCHER *watcher);


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
    WATCHER_TYPE *cli_watcher = &watcher_types[CLI_WATCHER_TYPE];
    (*cli_watcher).start(cli_watcher, NULL);

    int cur_length = 128;
    int bytes_read = 0;
    char *input = malloc(cur_length); //calloc(cur_length,1); // need to be dynamically resized
    int ret = 0;
    int ticker_print = 1;
    int stop = 0;
    while(1){ // as long as the user did not quit the program keep listening for input
        if(ticker_print == 1){
            printf("ticker> ");
            fflush(stdout);
        }
        // Waits for user input with current process suspended
        memset(input+bytes_read,0,cur_length);
        while((ret = read(STDIN_FILENO,input+bytes_read,1)) > 0){
            bytes_read += ret;
            if(input[bytes_read-1] == '\n') break;
            if(bytes_read >= cur_length){
                input = realloc(input,cur_length*2);
                cur_length *=2;
            }
        }
        if(ret == -1 && errno != EWOULDBLOCK) perror("Reading input failed");
        if(errno == EWOULDBLOCK) { // in the case where read has read everything or there is not input
            sigsuspend(&set); // stop until input is read
            ticker_print = 0;
            continue;
        }
        if(ret == 1){
            memset(input + bytes_read, 0, cur_length - bytes_read);
            evaluate(input+stop,&stop);
            ticker_print = 1;
        }
        if(ret == 0 ){ // ret is 0 means EOF
            free(input);
            exit(0);
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

void evaluate(char input[],int *stop){

    char *arg = input;
    char *ptr = input;
    int counter =0;
    while(*arg != '\n'){ // parse the command
        arg++;
        counter++;
    }
    input[counter] = '\0';
    char *command = strtok(ptr," ");
    char *args = strtok(NULL,"");
    if(args != NULL){
        if(strcmp(command, "start") == 0){
            printf("start\n");
        }else if(strcmp(command,"show") == 0){
            printf("show\n");
        }else if(strcmp(command, "trace") == 0){
            printf("trace\n");
        }else if(strcmp(command, "untrace") == 0){
            printf("untrace\n");
        }else if(strcmp(command,"stop") == 0){
            printf("stop\n");
        }else {
            printf("???\n");
        }
    }else{
         if(strcmp(command, "watchers") == 0){
            printf("watchers\n");
        }else if(strcmp(command, "quit") == 0){
        }else{
            printf("???\n");
        }
    }
    input[counter] = '\n';
    ptr = arg+1;
    *stop = counter+1 + *stop;
}

void add_to_table(WATCHER *watcher){

}