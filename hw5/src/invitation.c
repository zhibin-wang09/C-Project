#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <stdio.h>
#include <pthread.h>
#include <pthread.h>
#include <debug.h>

#include "client_registry.h"
#include "client.h"
#include "invitation.h"

struct invitation{
	CLIENT *source;
	CLIENT *target;
	GAME_ROLE source_role;
	GAME_ROLE target_role;
	INVITATION_STATE state;
	GAME *game;
	int reference_count;
	pthread_mutex_t lock;
};


INVITATION *inv_create(CLIENT *source, CLIENT *target, GAME_ROLE source_role, GAME_ROLE target_role){
	if(source == target) return NULL;
	INVITATION *invite = calloc(1,sizeof(INVITATION)); // initial a zero heap space
	invite -> source = source;
	client_ref(source,"as source of invitation\n");
	invite -> target = target;
	client_ref(target, "as target of invitation\n");
	invite -> state = INV_OPEN_STATE;
	invite -> source_role = source_role;
	invite -> target_role = target_role;
	pthread_mutex_init(&(invite -> lock), NULL);
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
	debug("Decrease reference count on invitation (%d -> %d) %s",inv->reference_count+1, inv->reference_count,why);
	if(inv->reference_count == 0){// if the reference count is zero then free the invitation
		 if(inv->game) game_unref(inv->game,"invitation is freed\n");
		 if(inv->source) client_unref(inv->source,"invitation is freed\n");
		 if(inv->target) client_unref(inv->target,"invitation is freed\n");
		 pthread_mutex_unlock(&inv->lock);
		 pthread_mutex_destroy(&inv->lock);
		 free(inv);
		 return;
	}
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
	if(inv == NULL) return -1;
	pthread_mutex_lock(&inv->lock);
	if(inv->state != INV_OPEN_STATE) {pthread_mutex_unlock(&inv->lock);return -1;}
	inv->state = INV_ACCEPTED_STATE; // change to accepted state
	GAME *new_game = game_create(); // create a new game
	if(!new_game) {pthread_mutex_unlock(&inv->lock);return -1;}
	inv->game = new_game;
	pthread_mutex_unlock(&inv->lock);
	return 0;
}

int inv_close(INVITATION *inv, GAME_ROLE role){
	if(inv == NULL) return -1;
	pthread_mutex_lock(&inv->lock);
	if(inv->state == INV_CLOSED_STATE){pthread_mutex_unlock(&inv->lock); return -1;}
	if(inv->state != INV_ACCEPTED_STATE && role != NULL_ROLE) {pthread_mutex_unlock(&inv->lock);return -1;} // can't resign a game with no game in progress
	if(role == NULL_ROLE && inv->state == INV_ACCEPTED_STATE) {pthread_mutex_unlock(&inv->lock);return -1;} // if null role is passed there can not be a game in progress
	if(inv->state == INV_ACCEPTED_STATE){ // only resign if the game is in progress
		if(game_resign(inv->game,role)){ pthread_mutex_unlock(&inv->lock); return -1;} // if game resign reuslt in error otherwise resign
		debug("game resigned");
	}
	inv->state = INV_CLOSED_STATE; // change to close state
	debug("game closed");
	//inv->game = NULL;
	pthread_mutex_unlock(&inv->lock);
	return 0;
}