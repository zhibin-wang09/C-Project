#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <stdio.h>
#include <pthread.h>
#include <pthread.h>
#include <semaphore.h>
#include <debug.h>

#include "client_registry.h"
#include "client.h"
#include "invitation.h"

struct invitation{
	CLIENT *source;
	CLIENT *target;
	int source_role;
	int target_role;
	INVITATION_STATE state;
	GAME *game;
	int reference_count;
	pthread_mutex_t lock;
	sem_t semaphore;
};


INVITATION *inv_create(CLIENT *source, CLIENT *target, GAME_ROLE source_role, GAME_ROLE target_role){
	if(source == target) return NULL;
	INVITATION *invite = calloc(sizeof(INVITATION), 1); // initial a zero heap space
	invite -> source = source;
	client_ref(source,"as source of invitation\n");
	invite -> target = target;
	client_ref(target, "as target of invitation\n");
	invite -> state = INV_OPEN_STATE;
	invite -> source_role = source_role;
	invite -> target_role = target_role;
	pthread_mutex_init(&(invite -> lock), NULL);
	sem_init(&(invite->semaphore), 0 ,0);
	inv_ref(invite,"for newly created invite\n");
	return invite;
}


INVITATION *inv_ref(INVITATION *inv, char *why){
	if(inv == NULL) return NULL;
	pthread_mutex_lock(&inv->lock);
	inv->reference_count++; // increase the invitation count
	debug("Increase reference count on invitation (%d -> %d) %s",inv->reference_count-1, inv->reference_count,why);
	pthread_mutex_unlock(&inv->lock);
	return inv;
}

void inv_unref(INVITATION *inv, char *why){
	if(inv == NULL) return;
	pthread_mutex_lock(&inv->lock);
	inv->reference_count--; // increase the invitation count
	if(inv->reference_count == 0){// if the reference count is zero then free the invitation
		 client_unref(inv->source, "invitation is freed\n");
		 client_unref(inv->target, "invitation is freed\n");
		 if(!inv->game) game_unref(inv->game,"invitation is freed\n");
		 free(inv);
	}
	debug("Decrease reference count on invitation (%d -> %d) %s",inv->reference_count, inv->reference_count+1,why);
	pthread_mutex_unlock(&inv->lock);
}

CLIENT *inv_get_source(INVITATION *inv){
	if(inv == NULL) return NULL;
	return inv->source;
}

CLIENT *inv_get_target(INVITATION *inv){
	if(inv == NULL) return NULL;
	return inv->target;
}

GAME_ROLE inv_get_source_role(INVITATION *inv){
	if(inv == NULL) return -1;
	return inv->source_role;
}

GAME_ROLE inv_get_target_role(INVITATION *inv){
	if(inv == NULL) return -1;
	return inv->target_role;
}

GAME *inv_get_game(INVITATION *inv){
	if(inv == NULL) return NULL;
	return inv->game;
}

int inv_accept(INVITATION *inv){
	if(inv == NULL || inv->state == INV_OPEN_STATE) return -1;
	pthread_mutex_lock(&inv->lock);
	inv->state = INV_ACCEPTED_STATE; // change to accepted state
	GAME *new_game = game_create(); // create a new game
	if(!new_game) {pthread_mutex_unlock(&inv->lock);return -1;}
	pthread_mutex_unlock(&inv->lock);
	return 0;
}

int inv_close(INVITATION *inv, GAME_ROLE role){
	if(inv == NULL || inv->state == INV_CLOSED_STATE) return -1;
	pthread_mutex_lock(&inv->lock);
	inv->state = INV_CLOSED_STATE;
	if(!game_resign(inv->game,role)){ pthread_mutex_unlock(&inv->lock); return -1;}
	pthread_mutex_unlock(&inv->lock);
	return 0;
}