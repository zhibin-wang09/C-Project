#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "ticker.h"
#include <sys/wait.h>
#include "watcher_define.h"


void terminated(int sig, siginfo_t *act, void* context);
void msg_ready(int sig, siginfo_t *info, void * ucontext);
void quit_program(int sig, siginfo_t *info, void *context);
void fcntl_setup(int fd);
static NODE head = {0};
static int terminate_program = 0;


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

    struct sigaction quit= {0};
    quit.sa_sigaction = &quit_program;
    sigemptyset(&quit.sa_mask);
    if(sigaction(SIGINT, &quit, NULL) < 0){
        perror("Creating SIGINT failed");
        return -1;
    }

    fcntl_setup(STDIN_FILENO);

    // initialize head and add the cli watcher to table
    head = (NODE){-1,NULL,NULL};
    WATCHER_TYPE *cli_watcher = &watcher_types[CLI_WATCHER_TYPE];
    WATCHER *cli = (*cli_watcher).start(cli_watcher, NULL);
    add_to_table(cli);

    int cur_length = 128;
    int bytes_read = 0;
    char *input = malloc(cur_length); //calloc(cur_length,1); // need to be dynamically resized
    int ret = 0;
    int ticker_print = 1;
    int cur_bytes_read = 0;
    while(1){ // as long as the user did not quit the program keep listening for input
        if(ticker_print == 1){
            (*cli_watcher).send(NULL, "ticker> ");
            fflush(stdout);
        }
        // Waits for user input with current process suspended
        memset(input,0,cur_length);
        if(terminate_program) {free(input);exit(0);}
        while((ret = read(STDIN_FILENO,input+cur_bytes_read,1)) > 0){
            bytes_read += ret;
            cur_bytes_read += ret;
            if(input[cur_bytes_read-1] == '\n') break;
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
            memset(input + cur_bytes_read, 0, cur_length - cur_bytes_read);
            watcher_types[CLI_WATCHER_TYPE].recv(NULL,input);
            ticker_print = 1;
        }
        if(ret == 0 ){ // ret is 0 means EOF
            free(input);
            exit(0);
        }
        cur_bytes_read = 0;
    }
    return 0;
}
/*
    signal handler for notification of a message from a watcher process
*/
void msg_ready(int sig, siginfo_t *info, void * ucontext){
    if(info->si_fd != 0){ // if notification is not from user input
        // search for the watcher with this si_fd as their parent_inputfd
        NODE *ptr = head.next;
        while(ptr != NULL){
            if(ptr -> watcher -> parent_inputfd == info -> si_fd){break;}
            ptr = ptr -> next;
        }
        int i= 1;
        // searches through watcher table to find corresponding watcher type
        for(i = 1; watcher_types[i].name != NULL && strcmp(watcher_types[i].name, ptr->watcher->name); i++);

        //write the data received into a buffer

        //pass the input to recv
        watcher_types[i].recv(ptr -> watcher,"");

    }
}

/*
    signal handler for termination of a watcher proces
*/
void terminated(int sig, siginfo_t *act, void *context){
    //reap the terminated child
    int status =0;
    pid_t pid = 0;
    // clear up all the zombie processes
    while((pid =waitpid(-1, &status, WNOHANG)) > 0){}
}

void quit_program(int sig, siginfo_t *info, void *context){
    NODE *ptr = head.next -> next;
    while(ptr != NULL){
        pid_t pid = (ptr -> watcher) -> pid;
        remove_from_table(ptr -> index);
        kill(pid, SIGTERM);
        ptr = ptr -> next;
    }
    terminate_program = 1;
}


void fcntl_setup(int fd){
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

void add_to_table(WATCHER *watcher){
    NODE *ptr = head.next;
    NODE *prev = &head;
    int index = head.index;
    while(ptr != NULL){ // search through the list to find the lowest index
        if(ptr -> index - index > 1) break;
        prev = ptr;
        index = ptr -> index;
        ptr = ptr -> next;
            }
    NODE *new = malloc(sizeof(new));
    new -> index = index+1;
    new -> watcher = watcher;
    new -> next = ptr;
    prev -> next = new;
    watcher->index = index+1;
}


/**
 * This function loops through the table and prints every watcher in the table
 * */
void print_table(){
    NODE *ptr = head.next;
    while(ptr != NULL){
        WATCHER *watcher = ptr -> watcher;
        watcher_types[CLI_WATCHER_TYPE].send(watcher, NULL);
        ptr = ptr -> next;
    }
}

void remove_from_table(int index){
    NODE *ptr = head.next;
    NODE *slow = &head;
    while(ptr != NULL){
        if(ptr->index != index){slow = ptr; ptr  = ptr -> next;}
        else break;
    }
    NODE *next = ptr -> next;
    slow -> next = next;
    free((ptr -> watcher )-> args);
    free(ptr -> watcher);
    free(ptr);
}

WATCHER *search_table(int index){
    NODE *ptr = head.next;
    while(ptr != NULL){
        if(ptr->index != index){ptr  = ptr -> next;}
        else {return (ptr -> watcher);}
    }
    return NULL;
}