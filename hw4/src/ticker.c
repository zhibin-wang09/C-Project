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
//char *dynamic_resize(char input[],int cur_size);

int ticker(void) {
    sigset_t set;
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
    int cur_length = 128;
    char input[cur_length]; //calloc(cur_length,1); // need to be dynamically resized
   // int bytes_read = 0;
    int ret = 0;
    while(1){ // as long as the user did not quit the program keep listening for input
        printf("ticker> ");
        fflush(stdout);
        // Waits for user input with current process suspended
        if((ret = read(STDIN_FILENO,input,cur_length)) < 0 && errno != EAGAIN){
            perror("Reading input failed");
        }
        printf("%d\n",ret);
        sigsuspend(&set);
        // while(bytes_read = (read(STDIN_FILENO,input,cur_length))!=0){
        //     if(bytes_read == -1){
        //         perror("Reading input failed");
        //     }
        //     int input_length = strlen(input);
        //     if(input_length >= sizeof(input)){
        //                     printf("%s\n",input);
        //         input = dynamic_resize(input,cur_length);
        //         cur_length*=2;
        //     }
        // } // read whatever
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


// char *dynamic_resize(char input[],int cur_size){
//     char *new_input_buffer = realloc(input, cur_size *2);
//     memset(new_input_buffer, 0, cur_size*2); // Clear the buffer
//     memcpy(new_input_buffer,input,cur_size);
//     return new_input_buffer;
// }