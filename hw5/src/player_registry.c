#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <debug.h>
#include "player_registry.h"
#include "client_registry.h"
#include "jeux_globals.h"


struct player_registry{
	PLAYER **players;
	pthread_mutex_t lock;
	int num_players;
	int registry_size;
};

PLAYER_REGISTRY *preg_init(void){
	PLAYER_REGISTRY *preg_register = calloc(1,sizeof(PLAYER_REGISTRY));
	preg_register -> players = calloc(3,sizeof(PLAYER*)); // initialize to a dynamic registry if initial size of 2
	preg_register -> num_players = 0;
	preg_register -> registry_size = 2;
	(preg_register->players)[preg_register->registry_size] = NULL; // last index is null
	pthread_mutex_init(&preg_register->lock,NULL);
	debug("player_registry is created\n");
	return preg_register;
}

void preg_fini(PLAYER_REGISTRY *preg){
	if(preg == NULL) return;
	pthread_mutex_lock(&preg->lock);
	for(int i=0;i<preg->num_players;i++){
		if(preg->players[i] != NULL){ // free what ever players that are remaining.
			player_unref(preg->players[i],"because player registry is being finalized\n");
		}
	}
	pthread_mutex_unlock(&preg->lock);
	pthread_mutex_destroy(&preg->lock);
	debug("player registry finalized\n");
	free(preg->players);
	free(preg);
}

PLAYER *preg_register(PLAYER_REGISTRY *preg, char *name){
	//search for any existing player
	pthread_mutex_lock(&preg->lock);
	for(int i=0;i<preg->num_players;i++){
		if(preg->players[i] != NULL && strlen(name) == strlen(player_get_name(preg->players[i])) && !strncmp(player_get_name(preg->players[i]),name,strlen(name))){
			player_ref(preg->players[i],"returned by player registry\n");
			pthread_mutex_unlock(&preg->lock);
			return preg->players[i];
		}
	}

	if(preg->registry_size == preg->num_players){
		preg->players = realloc(preg->players,(preg->registry_size+5 )* sizeof(PLAYER*)); // increase registry size by 5
		preg->registry_size += 4; // last index is null so the actual size is += 5 -1
		preg->players[preg->registry_size] = NULL;
		debug("new registry size = %d\n",preg->registry_size); // last index is null
	}
	// not found then create a new one
	PLAYER *player = player_create(name);
	(preg->players)[preg->num_players] = player; // insert at the last position
	player_ref(player,"reference retained by player registry\n");
	preg->num_players++;
	pthread_mutex_unlock(&preg->lock);
	return player;
}