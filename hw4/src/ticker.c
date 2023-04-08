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
void fctnl_setup(int fd);
void evaluate(char input[]);
void add_to_table(WATCHER *watcher);
void print_table();
WATCHER *search_table(int index);
void remove_from_table(int index);
static NODE head = {0};


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
            printf("ticker> ");
            fflush(stdout);
        }
        // Waits for user input with current process suspended
        memset(input,0,cur_length);
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
            evaluate(input);
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
    //write(STDIN_FILENO, "msg ready\n",10);
}

/*
    signal handler for termination of a watcher proces
*/
void terminated(int sig, siginfo_t *act, void *context){
    //reap the terminated child
    // pid_t pid = act -> si_pid;
    // int status =0;
    // // clear up all the zombie processes
    // while(){

    // }
}

void quit_program(int sig, siginfo_t *info, void *context){

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

void evaluate(char input[]){

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
            if(!strcmp(args,"CLI")) printf("???\n"); // no watcher of request type is found
            else{
                char *socket_name = strtok(args, " "); // the websocket name
                int i =0;
                for(i = 0; watcher_types[i].name != NULL; i++){
                    if(!strcmp(watcher_types[i].name,socket_name)) break;
                } // searches through watcher table to find corresponding watcher type
                if(watcher_types[i].name == NULL){ printf("???\n");} // no watcher of request type found
                else {
                    char *all_channels = strtok(NULL, "\n"); // input could have more than 1 channel
                    if(all_channels == NULL){ // no channel
                        printf("???\n");
                    }else{
                        char *channel = strtok(all_channels," "); // we only want the first one
                        if(channel == NULL) printf("???\n");
                        else {
                            WATCHER_TYPE type = watcher_types[i];
                            char *channel_args[1] = {channel};
                            WATCHER *watcher = type.start(&type,channel_args);
                            add_to_table(watcher);
                        }
                    }
                }
            }
        }else if(strcmp(command,"show") == 0){
            printf("show\n");
        }else if(strcmp(command, "trace") == 0){
            printf("trace\n");
        }else if(strcmp(command, "untrace") == 0){
            printf("untrace\n");
        }else if(strcmp(command,"stop") == 0){
            int index = 0;
            char *ptr = args;
            int invalid = 0;
            while(*ptr != 0){
                if(*ptr <= '0' || *ptr >'9'){ printf("???\n"); invalid = 1; break;} // read index number while validating
                index += *ptr - '0';
                ptr++;
            }
            if(invalid != 1){ // input is valid
                WATCHER *del = search_table(index);
                if(del == NULL || index == 0){
                    printf("???\n");
                }else{
                    remove_from_table(index);
                    int i =0;
                    for(i = 0; watcher_types[i].name != NULL && watcher_types[i].name != del->name; i++); // searches through watcher table to find corresponding watcher type
                    WATCHER_TYPE type = watcher_types[i];
                    type.stop(del);
                }
            }
        }else {
            printf("???\n");
        }
    }else{
         if(strcmp(command, "watchers") == 0){
            print_table();
        }else if(strcmp(command, "quit") == 0){
        }else{
            printf("???\n");
        }
    }
    input[counter] = '\n';
    //ptr = arg+1;
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
}


/**
 * This function loops through the table and prints every watcher in the table
 * */
void print_table(){
    NODE *ptr = head.next;
    while(ptr != NULL){
        WATCHER *watcher = ptr -> watcher;

        char *name = watcher -> name;
        if(strcmp(name, watcher_types[CLI_WATCHER_TYPE].name) == 0){ printf("%d\t%s(%d,%d,%d)\n",ptr->index,name, watcher->pid,watcher->parent_inputfd,watcher->parent_outputfd);}
        else{
            printf("%d\t%s(%d,%d,%d)\n",ptr->index,name, watcher->pid,watcher->parent_inputfd,watcher->parent_outputfd);
        }
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
}

WATCHER *search_table(int index){
    NODE *ptr = head.next;
    while(ptr != NULL){
        if(ptr->index != index){ptr  = ptr -> next;}
        else {return (ptr -> watcher);}
    }
    return NULL;
}