#include <stdlib.h>
#include <stdio.h>

#include "ticker.h"
#include "bitstamp.h"
#include "debug.h"
#include "watcher_define.h"
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

extern int fcntl_setup(int fd);

WATCHER *bitstamp_watcher_start(WATCHER_TYPE *type, char *args[]) {
    pid_t watcher_pid;
    int child_to_parent[2]; // first pipe: fd[0] is the read end for the parent, fd[1] is the write end for the child
    int parent_to_child[2]; // second pipe : fd[0] is the read end for the child, fd[1] is the write end for the parent
    if(pipe(child_to_parent) < 0){perror("Creating first pipe failed");}
    if(pipe(parent_to_child) < 0){perror("Creating second pipe failed");}
    if((watcher_pid = fork()) == 0){
        close(child_to_parent[0]); // close read of first pipe
        close(parent_to_child[1]); // close write of second pipe
        dup2(child_to_parent[1],STDOUT_FILENO);
        dup2(parent_to_child[0], STDIN_FILENO);
        close(child_to_parent[1]);//close child_to_parent[1]
        close(parent_to_child[0]);//close parent_to_child[0]
        if(execvp((type->argv)[0], type -> argv) == -1){ perror("Creating watcher failed");}
    }

    close(child_to_parent[1]);
    close(parent_to_child[0]);
    WATCHER *bitstamp_watcher = malloc(sizeof(WATCHER));
    bitstamp_watcher -> pid = watcher_pid;
    bitstamp_watcher -> enable = 0;
    bitstamp_watcher -> parent_inputfd = child_to_parent[0];
    bitstamp_watcher -> parent_outputfd = parent_to_child[1];
    bitstamp_watcher -> child_inputfd = parent_to_child[0];
    bitstamp_watcher -> child_outputfd = child_to_parent[1];
    bitstamp_watcher -> name = type -> name;
    fcntl_setup(bitstamp_watcher -> parent_inputfd);
    size_t size_of_arg =0;
    char *c = *args;
    while(*c != '\0'){c++; size_of_arg++;};
    size_of_arg++;
    char *copy_args = calloc(size_of_arg,1);
    memcpy(copy_args, args[0],size_of_arg);
    bitstamp_watcher -> args = copy_args;
    char buf[128];
    snprintf(buf, sizeof(buf),"{ \"event\": \"bts:subscribe\", \"data\": { \"channel\": \"%s\" } }\n",bitstamp_watcher->args);
    bitstamp_watcher_send(bitstamp_watcher,buf);
    return bitstamp_watcher;
}

int bitstamp_watcher_stop(WATCHER *wp) {
    pid_t pid = wp->pid;
    if (kill(pid,SIGTERM) < 0){
        perror("kill failed was returned\n");
        return -1;
    }
    close(wp -> parent_inputfd);
    close(wp -> parent_outputfd);
    return 0;
}

int bitstamp_watcher_send(WATCHER *wp, void *arg) {
    char *msg = (char *)arg;
    if(write(wp->parent_outputfd,msg,strlen(msg)) < 0){
        perror("Connecting to watcher failed");
        return -1;
    }
    return 0;
}

int bitstamp_watcher_recv(WATCHER *wp, char *txt) {
    printf("%s\n",txt);
    // char output[258];
    // int ret = read(info->si_fd, output, sizeof(output));
    // printf("ret: %d, %s\n",ret,output);
    return 0;
}

int bitstamp_watcher_trace(WATCHER *wp, int enable) {
    wp->enable = enable;
    return 0;
}
