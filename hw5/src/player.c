#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "debug.h"
#include "player.h"

struct player{
	char *username;
	int rating;
	pthread_mutex_t lock;
	int reference_count;
};

PLAYER *player_create(char *name){
	PLAYER *player = calloc(1,sizeof(PLAYER));
	player -> username = calloc(1,strlen(name)+1); // private copy string space
	player->username = strncpy(player->username, name, strlen(name)); // copy the string over, strlen() does not include null terminator
	player -> rating = PLAYER_INITIAL_RATING;
	pthread_mutex_init(&player->lock,NULL);
	player->reference_count = 0;
	player_ref(player,"newly created player");
	return player;
}

PLAYER *player_ref(PLAYER *player, char *why){
	if(player == NULL) return NULL;
	pthread_mutex_lock(&player->lock);
	player->reference_count++;
	debug("Increase reference count on player (%d -> %d) %s",player->reference_count-1, player->reference_count,why);
	pthread_mutex_unlock(&player->lock);
	return NULL;
}

void player_unref(PLAYER *player, char *why){
	if(player == NULL) return;
	pthread_mutex_lock(&player->lock);
	player->reference_count--;
	debug("decrease reference count on player (%d -> %d) %s",player->reference_count+1, player->reference_count,why);
	if(player->reference_count ==0){
		free(player->username);
		pthread_mutex_unlock(&player->lock);
		pthread_mutex_destroy(&player->lock);
		free(player);
		return;
	}
	pthread_mutex_unlock(&player->lock);
}

char *player_get_name(PLAYER *player){
	if(player ==NULL) return NULL;
	return player->username;
}

int player_get_rating(PLAYER *player){
	if(player ==NULL) return -1;
	return player->rating;
}

void player_post_result(PLAYER *player1, PLAYER *player2, int result){
	if(!player1 || !player2) return;
	pthread_mutex_lock(&player1 -> lock);
	pthread_mutex_lock(&player2->lock);
	double r1 = player1->rating;
	double r2 = player2->rating;
	double s1, s2;
	switch(result){
		case 0:
			s1 = 0.5;
			s2 = 0.5;
			break;
		case 1:
			s1 = 1;
			s2 = 0;
			break;
		case 2:
			s2 = 1;
			s1 = 0;
			break;
		default:
			return;
	}
	double e1 = 1/(1 + pow(10,((r2-r1)/400)));
	double e2 = 1/(1 + pow(10,((r1-r2)/400)));
	player1->rating = r1 + 32*(s1-e1);
	player2->rating = r2 + 32*(s2-e2);
	pthread_mutex_unlock(&player1->lock);
	pthread_mutex_unlock(&player2->lock);
}