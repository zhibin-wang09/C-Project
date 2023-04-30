#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <debug.h>
#include "player_registry.h"
#include "client_registry.h"
#include "jeux_globals.h"


struct player_registry{
	PLAYER *players[MAX_CLIENTS];
	pthread_mutex_t lock;
};

PLAYER_REGISTRY *preg_init(void){
	PLAYER_REGISTRY *preg_register = calloc(1,sizeof(PLAYER_REGISTRY));
	pthread_mutex_init(&preg_register->lock,NULL);
	debug("player_registry is created\n");
	return preg_register;
}

void preg_fini(PLAYER_REGISTRY *preg){
	if(preg == NULL) return;
	pthread_mutex_lock(&preg->lock);
	for(int i= 0; i<MAX_CLIENTS;i++){
		if(preg->players[i] != NULL){ // free what ever players that are remaining.
			player_unref(preg->players[i],"because player registry is being finalized\n");
		}
	}
	pthread_mutex_unlock(&preg->lock);
	pthread_mutex_destroy(&preg->lock);
	debug("player registry finalized\n");
	free(preg);
}

PLAYER *preg_register(PLAYER_REGISTRY *preg, char *name){
	//search for any existing player
	pthread_mutex_lock(&preg->lock);
	for(int i=0;i<MAX_CLIENTS;i++){
		if(preg->players[i] != NULL && !strncmp(player_get_name(preg->players[i]),name,strlen(name))){
			player_ref(preg->players[i],"returned by player registry\n");
			pthread_mutex_unlock(&preg->lock);
			return preg->players[i];
		}
	}
	// not found then create a new one
	PLAYER *player = player_create(name);
	//find a empty spot and fill in the player;
	for(int i =0 ;i<MAX_CLIENTS;i++){
		if(preg->players[i] == NULL){
			preg->players[i] = player;
			player_ref(player,"reference retained by player registry\n");
			break;
		}
	}
	pthread_mutex_unlock(&preg->lock);
	return player;
}