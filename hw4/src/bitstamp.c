#include <stdlib.h>
#include <stdio.h>

#include "ticker.h"
#include "bitstamp.h"
#include "debug.h"
#include "watcher_define.h"
#include "store.h"
#include "argo.h"
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>

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
        fcntl_setup(STDIN_FILENO);
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
    bitstamp_watcher -> serial_num = 0;
    bitstamp_watcher -> price_store = NULL;
    bitstamp_watcher -> volume_store = NULL;
    fcntl_setup(bitstamp_watcher -> parent_inputfd);

    int size_args = 0;
    char **ptr = args;
    while(*ptr != 0){
        size_args++;
        ptr++;
    }
    int num_strings = size_args + 1; // lenght of the array + null
    char **copy = calloc(num_strings * 8,1); // allocate space for the array
    char **point = copy;
    for( int i =0; i< num_strings; i++){
        if(i == num_strings -1 ){*copy = 0;break;}
        *copy = calloc(1,strlen(args[i])+1);
        strncpy(*copy, args[i],strlen(args[i]));
        copy++;
    }
    bitstamp_watcher -> args = point;
    char buf[258];
    snprintf(buf, sizeof(buf),"{ \"event\": \"bts:subscribe\", \"data\": { \"channel\": \"%s\" } }\n",*(bitstamp_watcher->args));
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
    //printf("enable: %d\n",wp->enable);
    wp -> serial_num += 1;
    if(wp -> enable){
        struct timespec time;
        clock_gettime(CLOCK_REALTIME,&time);
        if(txt == NULL){perror("Unable to parse"); return -1;}
        fprintf(stderr, "[%ld.%06ld][%s][%2d][%5d]: ", time.tv_sec, time.tv_nsec/1000, wp->name, wp -> parent_inputfd, wp->serial_num);
        fflush(stderr);
        //if(txt[0]=='\b'){txt = txt+2;}
        fprintf(stderr,"%s",txt);
    }
    if(strstr(txt,"Server message:")){
        char *json = strstr(txt, "{"); // start of json format
        FILE *stream;
        if((stream = (fmemopen(json,strlen(json),"r"))) == NULL){perror("Unable to create stream"); return -1;}
        ARGO_VALUE *parse = argo_read_value(stream);
        if(parse == NULL){perror("Unable to parse the json"); return -1;}
        ARGO_VALUE *event_argo = argo_value_get_member(parse,"event");
        if(event_argo == NULL){perror("Unable to find the event member"); return -1;}
        char *event = argo_value_get_chars(event_argo);

        if(!strcmp(event, "trade")){ // event is trade then extract price and amount
            ARGO_VALUE *data = argo_value_get_member(parse,"data"); // the data object
            ARGO_VALUE *amount_argo = argo_value_get_member(data,"amount");
            ARGO_VALUE *price_argo = argo_value_get_member(data,"price");
            double amount;
            argo_value_get_double(amount_argo,&amount);
            double price;
            argo_value_get_double(price_argo,&price);
            char buf1[128];
            snprintf(buf1, sizeof(buf1),"bitstamp.net:%s:price",*(wp->args));
            struct store_value price_store = {.type=STORE_DOUBLE_TYPE, .content.double_value=price};
            store_put(buf1,&price_store);
            char buf2[128];
            snprintf(buf2, sizeof(buf2),"bitstamp.net:%s:volume",*(wp->args));
            struct store_value *previous_volume_store;
            if((previous_volume_store = store_get(buf2) )== NULL){ // initial volume = amount
                struct store_value amount_store = {.type=STORE_DOUBLE_TYPE, .content.double_value=price};
                wp->volume_store = &amount_store;
                store_put(buf2, &amount_store);
            }else{ // accumulating volume = previous volumn + amount
                previous_volume_store -> content.double_value = previous_volume_store -> content.double_value + amount;
                store_put(buf2,previous_volume_store);
                store_free_value(previous_volume_store);
            }
            wp->price_store = &price_store;
        }
        free(event);
        argo_free_value(parse);
        fclose(stream);
    }
    return 0;
}

int bitstamp_watcher_trace(WATCHER *wp, int enable) {
    wp -> enable = enable;
    return 0;
}
