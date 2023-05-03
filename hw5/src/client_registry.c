#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <stdio.h>
#include <pthread.h>

#include "debug.h"
#include "client_registry.h"
#include "client.h"
#include "player.h"

struct client_registry{
	CLIENT *list_of_clients[MAX_CLIENTS]; // an dynamic  array of clients
	int current_capacity;
	pthread_mutex_t lock; // a lock to assist synchronization
	sem_t semaphore; // used to assist with the waiting for client to shutdown
} client_registry;

CLIENT_REGISTRY *creg_init(){
	// initialize heap space for the registry
	CLIENT_REGISTRY *registry = calloc(1,sizeof(CLIENT_REGISTRY));
	memset(registry -> list_of_clients, 0, sizeof(registry -> list_of_clients)); // initialize everything to 0
	pthread_mutex_init(&(registry -> lock), NULL);
	registry->current_capacity = 0;
	sem_init(&(registry->semaphore), 0 ,0);
	debug("client registry created");
	return registry;
}

void creg_fini(CLIENT_REGISTRY *cr){
	if(cr){
		pthread_mutex_destroy(&(cr->lock));
		free(cr);
	}
	cr = NULL;
}

CLIENT *creg_register(CLIENT_REGISTRY *cr, int fd){
	if(cr == NULL) return NULL; // invalid
	CLIENT *client = client_create(cr,fd);
	if(!client) return NULL;
	// locks the code so only one thread can manipulate shared varaibles
	pthread_mutex_lock(&(cr -> lock));
	int i;
	for(i =0;i<MAX_CLIENTS; i++){ // find the lowest index with available spot
		if((cr->list_of_clients)[i] == 0) break;
	}
	(cr->list_of_clients)[i] = client;
	cr->current_capacity = cr->current_capacity+1;
	//client_ref(client, "client is registered and returned");
	pthread_mutex_unlock(&(cr -> lock));

	return client;
}

int creg_unregister(CLIENT_REGISTRY *cr, CLIENT *client){
	if(cr == NULL) return -1;
	pthread_mutex_lock(&(cr -> lock));
	int index =0;
	for(index = 0;index < MAX_CLIENTS; index++){
		if((cr->list_of_clients)[index] == client) break;
	}
	if((cr->list_of_clients)[index] == 0){ pthread_mutex_unlock(&(cr -> lock)); return -1;} // no corresponding client found
	client_unref(client, "reference of client is unregistered");
	cr->current_capacity = cr->current_capacity-1;
	if(!cr->current_capacity) sem_post(&cr->semaphore); // increase semaphore to allow wait_for_empty because no more clients
	(cr->list_of_clients)[index] = 0;
	pthread_mutex_unlock(&(cr -> lock));
	return 0;
}

CLIENT *creg_lookup(CLIENT_REGISTRY *cr, char *user){
	if(cr == NULL) return NULL;
	pthread_mutex_lock(&(cr -> lock));
	int index =0;
	for(index = 0;index < MAX_CLIENTS; index++){ //loop through the registry array
		if(!(cr->list_of_clients)[index]) continue; // if the index does not contain client then skip
		PLAYER *p = client_get_player((cr->list_of_clients)[index]); // get the client's name
		char *p_username = player_get_name(p); // get the name of the player
		if(!strcmp(p_username, user)){ // if name is the same then return client
			client_ref((cr->list_of_clients)[index], "reference increased by client lookup");
			CLIENT *lookup  = (cr->list_of_clients)[index];
			pthread_mutex_unlock(&(cr -> lock));
			return lookup;
		}
	}
	pthread_mutex_unlock(&(cr->lock));
	return NULL;
}

PLAYER **creg_all_players(CLIENT_REGISTRY *cr){
	if(cr == NULL) return NULL;
	pthread_mutex_lock(&(cr -> lock));
	PLAYER **players = calloc(cr->current_capacity+1, sizeof(PLAYER*));
	int index =0;
	for(int i =0; i < MAX_CLIENTS; i++){
		if(!(cr->list_of_clients)[i]) continue; // if the index does not contain client then skip
		players[index] = client_get_player(( cr->list_of_clients)[i]);
		player_ref(players[index],"reference being added to player list");
		index++;
	}
	pthread_mutex_unlock(&(cr->lock));
	return players;
}

void creg_wait_for_empty(CLIENT_REGISTRY *cr){
	sem_wait(&cr->semaphore);
	sem_post(&cr->semaphore); // to allow for othert concurrent threads to proceed because there are
							  // zero clients so all threads that was blocked can proceeds
}

void creg_shutdown_all(CLIENT_REGISTRY *cr){
	if(cr == NULL) return;
	pthread_mutex_lock(&cr->lock);
	int already_zero = 1; // indicator that the registry is already 0
	for(int i = 0; i< MAX_CLIENTS;i++){
		if(!(cr->list_of_clients)[i]) continue; // if the index does not contain client then skip
		shutdown(client_get_fd(cr->list_of_clients[i]), SHUT_RD);
		already_zero =0; // not zero
	}
	if(already_zero) sem_post(&cr->semaphore);
	pthread_mutex_unlock(&cr->lock);
}
