#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>

#include "debug.h"
#include "protocol.h"
#include "server.h"
#include "client_registry.h"
#include "player_registry.h"
#include "jeux_globals.h"

#ifdef DEBUG
int _debug_packets_ = 1;
#endif

static void terminate(int status);
void terminal_is_closed(int sig, siginfo_t *act, void* context);
int open_listenfd(char *port);
int listenfd;

/*
 * "Jeux" game server.
 *
 * Usage: jeux <port>
 */
int main(int argc, char* argv[]){
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.
    int port;
    if(argc != 3) return -1; // no -p <port>
    if(strcmp(argv[1],"-p")) return -1; // option is not -p
    port = (int)strtol(argv[2],NULL,10);
    if(port < 1024) return -1; // port is declared int the range of reserved ports

    struct sigaction terminal_closed = {0};
    terminal_closed.sa_sigaction = terminal_is_closed; // SIGHUP handler
    sigemptyset(&terminal_closed.sa_mask);
    if(sigaction(SIGHUP, &terminal_closed,NULL) < 0){ // install handler
        return -1;
    }

    // struct sigaction process_interrupted = {0};
    // process_interrupted.sa_sigaction = terminal_is_closed; // SIGHUP handler
    // sigemptyset(&process_interrupted.sa_mask);
    // if(sigaction(SIGHUP, &process_interrupted,NULL) < 0){ // install handler
    //     return -1;
    // }
    // Perform required initializations of the client_registry and
    // player_registry.
    client_registry = creg_init();
    player_registry = preg_init();

    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function jeux_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.

    if((listenfd = open_listenfd(argv[2])) < 0){
        return -1;
    }
    debug("server listening on port %d",port);
    int connfd;
    int *connfd_p;
    socklen_t clientaddr_len;
    struct sockaddr clientaddr;
    pthread_t tid;
    while(1){
        clientaddr_len = sizeof(struct sockaddr);
        //connfd = malloc(sizeof(int));
        if((connfd = accept(listenfd, &clientaddr, &clientaddr_len)) < 0){
            terminate(EXIT_FAILURE);
        }
        connfd_p = malloc(sizeof(int));
        *connfd_p = connfd;
        if(pthread_create(&tid, NULL, &jeux_client_service,connfd_p)){
            close(listenfd);
            close(*connfd_p);
            terminate(EXIT_FAILURE);
        }
        connfd = 0;

    }

    terminate(EXIT_SUCCESS);
}

void terminal_is_closed(int sig, siginfo_t *act, void* context){
    close(listenfd);
    terminate(EXIT_SUCCESS);
}

int open_listenfd(char *port){
    struct addrinfo hint, *res, *ptr;
    int listenfd, optval=1;
    memset(&hint, 0,sizeof(struct addrinfo));
    hint.ai_socktype = SOCK_STREAM; // SOCK_STREAM indicates that this socket will be used as an end point connection
    hint.ai_flags = AI_NUMERICSERV | AI_PASSIVE | AI_ADDRCONFIG; // indicate that socket uses port number as service, , is an listening socket, and is a connection
    if(getaddrinfo(NULL,port,&hint,&res)){ // get the list of addresses
        return -2;
    }

    for(ptr = res; ptr ; ptr = ptr -> ai_next){
        //creates a socket descriptor
        if((listenfd  = socket(ptr->ai_family, ptr -> ai_socktype, ptr -> ai_protocol)) < 0) continue; // create socket connection
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,    //line:netp:csapp:setsockopt
                   (const void *)&optval , sizeof(int));
        if(!bind(listenfd, ptr -> ai_addr, ptr -> ai_addrlen)) break; // create server side endpoint
        if(close(listenfd) < 0){ return -1;} // close listenfd because it is no longer needed since it didn't work with bind
    }

    freeaddrinfo(res); // necessary to free the return list
    if(!ptr){ // no address succeeded
        return -1;
    }

    if(listen(listenfd, 1024) < 0){
        close(listenfd);
        return -1;
    }
    return listenfd;
}

/*
 * Function called to cleanly shut down the server.
 */
void terminate(int status) {
    // Shutdown all client connections.
    // This will trigger the eventual termination of service threads.
    creg_shutdown_all(client_registry);
    
    debug("%ld: Waiting for service threads to terminate...", pthread_self());
    creg_wait_for_empty(client_registry);
    debug("%ld: All service threads terminated.", pthread_self());

    // Finalize modules.
    creg_fini(client_registry);
    preg_fini(player_registry);

    debug("%ld: Jeux server terminating", pthread_self());
    exit(status);
}
