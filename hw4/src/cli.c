#include <stdlib.h>

#include "ticker.h"
#include "store.h"
#include "cli.h"
#include "debug.h"
#include "watcher_define.h"
#include <string.h>
#include <signal.h>


WATCHER *cli_watcher_start(WATCHER_TYPE *type, char *args[]) {
    WATCHER  *cli_watcher = malloc(sizeof(WATCHER));
    if(cli_watcher == NULL){
        perror("Ran out of memory");
    }
    cli_watcher -> pid = -1;
    cli_watcher -> enable = 0;
    cli_watcher -> parent_inputfd = STDIN_FILENO;
    cli_watcher -> parent_outputfd = STDOUT_FILENO;
    cli_watcher -> name = type -> name;
    cli_watcher -> child_inputfd = -1;
    cli_watcher -> child_outputfd = -1;
    cli_watcher -> args = NULL;
    cli_watcher -> index= 0;
    return cli_watcher;
}

int cli_watcher_stop(WATCHER *wp) {
    return -1;
}

int cli_watcher_send(WATCHER *wp, void *arg) {
    // TO BE IMPLEMENTED
    if(wp == NULL){
        char *output = (char *) arg;
        fprintf(stdout,"%s",output);
    }else{
        char *name = wp -> name;
        if(strcmp(name, watcher_types[CLI_WATCHER_TYPE].name) == 0){ printf("%d\t%s(%d,%d,%d)\n",wp->index,name, wp->pid,wp->parent_inputfd,wp->parent_outputfd);}
        else{
            printf("%d\t%s(%d,%d,%d)",wp->index,name, wp->pid,wp->parent_inputfd,wp->parent_outputfd);
            int i =0;
            for(i = 0; watcher_types[i].name != NULL && watcher_types[i].name != wp->name; i++); // searches through watcher table to find corresponding watcher type
            WATCHER_TYPE type = watcher_types[i];
            char **arg = type.argv;
            while(*arg != NULL){
                printf(" %s", *arg);
                arg++;
            }
            printf(" [%s]\n",wp->args);
        }
    }
    return 0;
}

int cli_watcher_recv(WATCHER *wp, char *txt) {
    char *arg = txt;
    char *ptr = txt;
    int counter =0;
    while(*arg != '\n'){ // parse the command
        arg++;
        counter++;
    }
    txt[counter] = '\0';
    char *command = strtok(ptr," ");
    char *args = strtok(NULL,"");
    if(args != NULL){
        if(strcmp(command, "start") == 0){
            if(!strcmp(args,"CLI")) watcher_types[CLI_WATCHER_TYPE].send(NULL,"???\n"); // no watcher of request type is found
            else{
                char *socket_name = strtok(args, " "); // the websocket name
                int i =1;
                for(i = 1; watcher_types[i].name != NULL; i++){
                    if(!strcmp(watcher_types[i].name,socket_name)) break;
                } // searches through watcher table to find corresponding watcher type
                if(watcher_types[i].name == NULL){ watcher_types[CLI_WATCHER_TYPE].send(NULL,"???\n");} // no watcher of request type found
                else {
                    char *all_channels = strtok(NULL, "\n"); // txt could have more than 1 channel
                    if(all_channels == NULL){ // no channel
                        watcher_types[CLI_WATCHER_TYPE].send(NULL,"???\n");
                    }else{
                        char *channel = strtok(all_channels," "); // we only want the first one
                        if(channel == NULL) watcher_types[CLI_WATCHER_TYPE].send(NULL,"???\n");
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
            struct store_value *value= store_get(args);
            if(value == NULL){printf("???\n");}
            else{
                printf("%s\t%f",args,value->content.double_value);
            }
            store_free_value(value);
        }else if(strcmp(command, "trace") == 0 || strcmp(command, "untrace") == 0){
            int index = 0;
            char *ptr = args;
            int invalid = 0;
            while(*ptr != 0){
                if(*ptr <= '0' || *ptr >'9'){watcher_types[CLI_WATCHER_TYPE].send(NULL,"???\n"); invalid = 1; break;} // read index number while validating
                index += *ptr - '0';
                ptr++;
            }
            if(invalid != 1){
                WATCHER *target = search_table(index);
                if(target == NULL || index == 0){
                    watcher_types[CLI_WATCHER_TYPE].send(NULL,"???\n");
                }else{
                    int i =0;
                    for(i = 0; watcher_types[i].name != NULL && strcmp(watcher_types[i].name,target->name); i++); // searches through watcher table to find corresponding watcher type
                    WATCHER_TYPE type = watcher_types[i];
                    if(strcmp(command,"trace") == 0) type.trace(target,1);
                    else type.trace(target,0);
                }
            }
        }else if(strcmp(command,"stop") == 0){
            int index = 0;
            char *ptr = args;
            int invalid = 0;
            while(*ptr != 0){
                if(*ptr <= '0' || *ptr >'9'){watcher_types[CLI_WATCHER_TYPE].send(NULL,"???\n"); invalid = 1; break;} // read index number while validating
                index += *ptr - '0';
                ptr++;
            }
            if(invalid != 1){ // txt is valid
                WATCHER *del = search_table(index);
                if(del == NULL || index == 0){
                    watcher_types[CLI_WATCHER_TYPE].send(NULL,"???\n");
                }else{
                    int i =0;
                    for(i = 0; watcher_types[i].name != NULL && strcmp(watcher_types[i].name,del->name); i++); // searches through watcher table to find corresponding watcher type
                    WATCHER_TYPE type = watcher_types[i];
                    type.stop(del);
                    remove_from_table(index);
                }
            }
        }else {
            watcher_types[CLI_WATCHER_TYPE].send(NULL,"???\n");
        }
    }else{
         if(strcmp(command, "watchers") == 0){
            print_table();
        }else if(strcmp(command, "quit") == 0){
            kill(getpid(),SIGINT);
        }else{
            watcher_types[CLI_WATCHER_TYPE].send(NULL,"???\n");
        }
    }
    txt[counter] = '\n';
    return 0;
}

int cli_watcher_trace(WATCHER *wp, int enable) {
    return -1;
}
