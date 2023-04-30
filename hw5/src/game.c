#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <debug.h>

#include "game.h"

struct game{
	GAME_ROLE winner;
	int reference_count;
	pthread_mutex_t lock;
	char *current_state;
	char previous_move;
	int ended;
	int moves_made;
};

struct game_move{
	int placement; // from a range of ['1','9']
	char move; // could be X or O
};

GAME *game_create(void){
	GAME *game = calloc(1,sizeof(GAME));
	game->winner = NULL_ROLE;
	game->reference_count = 0;
	pthread_mutex_init(&game->lock, NULL);
	game->current_state = calloc(41,1);
	char *initial_state = " | | \n-----\n | | \n-----\n | | \nX to move\n";
	strncpy(game->current_state,initial_state,strlen(initial_state)+1); // include null terminator
	game->ended = 0;
	game->previous_move = 0;
	game->moves_made =0;
	game_ref(game,"newly created game\n");
	return game;
}

GAME *game_ref(GAME *game, char *why){
	if(game == NULL) return NULL;
	pthread_mutex_lock(&game->lock);
	game->reference_count++;
	debug("Increase reference count on game (%d -> %d) %s",game->reference_count-1, game->reference_count,why);
	pthread_mutex_unlock(&game->lock);
	return game;
}

void game_unref(GAME *game, char *why){
	if(game == NULL) return;
	pthread_mutex_lock(&game->lock);
	game->reference_count--;
	debug("Decrease reference count on game (%d -> %d) %s",game->reference_count+1, game->reference_count,why);
	if(game->reference_count == 0){
		free(game->current_state);
		pthread_mutex_unlock(&game->lock);
		pthread_mutex_destroy(&game->lock);
		free(game);
		return;
	}
	pthread_mutex_unlock(&game->lock);
}

int game_apply_move(GAME *game, GAME_MOVE *move){
	if(game == NULL) return -1;
	pthread_mutex_lock(&game->lock);
	if(game->ended){
		pthread_mutex_unlock(&game->lock);
		return -1;
	}
	int position = move->placement-1;
	char *state = game->current_state;
	debug("pos: %d, current move: %c, previous_move:%c\n",position,move->move,game->previous_move);
	if((game->previous_move != 0 && game->previous_move == move->move) || (game->previous_move==0 && move->move == 'O')){
		// O tries to move first or the same person tries to move twice in a row
		debug("move is illegal in current state\n");
		pthread_mutex_unlock(&game->lock);
		return -1;
	}
	if(position >= 0 && position <= 2){
		if(state[position*2] != ' '){
			debug("can't make a move on taken position\n");
			pthread_mutex_unlock(&game->lock);
			return -1;
		}
		state[position*2] = move->move;

		// check if this move lead to a winner
		int connected = 0;
		int start = 0;
		//horizontal
		for(int i =0; i<2;i++){
			if(state[start] == ' ')break;
			if(state[(start +1)*2] == state[(start)*2])connected++;
			start++;
		}
		if(connected == 2){
			game->winner = move->move == 'X' ? FIRST_PLAYER_ROLE :SECOND_PLAYER_ROLE;
			game->ended=1;
		}
		connected = 0;
		start =0;
		//vertical
		for(int i =0; i<2;i++){
			if(state[start] == ' ')break;
			if(state[start + 12] == state[start]) connected++;
			start+=12;
		}
		if(connected == 2){
			game->winner = move->move == 'X' ? FIRST_PLAYER_ROLE :SECOND_PLAYER_ROLE;
			game->ended=1;
		}
		connected = 0;
		start =0;
		//diagonal
		for(int i =0; i<2;i++){
			if(state[start] == ' ')break;
			if(state[start + 14] == state[start]) connected++;
			start+=14;
		}
		if(connected == 2){
			game->winner = move->move == 'X' ? FIRST_PLAYER_ROLE :SECOND_PLAYER_ROLE;
			game->ended=1;
		}
		connected = 0;
		start =4;
		//counter diagonally
		for(int i =0; i<2;i++){
			if(state[start] == ' ')break;
			if(state[start + 10] == state[start]) connected++;
			start+=10;
		}
		if(connected == 2){
			game->winner = move->move == 'X' ? FIRST_PLAYER_ROLE :SECOND_PLAYER_ROLE;
			game->ended=1;
		}
	}else if(position >= 3 && position <= 5){
		int relative_pos = position - 3;
		if(state[12 + relative_pos*2] != ' '){
			debug("can't make a move on taken position\n");
			pthread_mutex_unlock(&game->lock);
			return -1;
		}
		state[12 + relative_pos * 2] = move->move;

		// check if this move lead to a winner
		int connected = 0;
		int start = 12;
		//horizontally
		for(int i =0; i<2;i++){
			if(state[start] == ' ')break;
			if(state[start+2] == state[start])connected++;
			start+=2;
		}
		if(connected == 2){
			game->winner = move->move == 'X' ? FIRST_PLAYER_ROLE :SECOND_PLAYER_ROLE;
			game->ended=1;
		}
		connected = 0;
		start =2;
		//vertically
		for(int i =0; i<2;i++){
			if(state[start] == ' ')break;
			if(state[start + 12] == state[start]) connected++;
			start+=12;
		}
		if(connected == 2){
			game->winner = move->move == 'X' ? FIRST_PLAYER_ROLE :SECOND_PLAYER_ROLE;
			game->ended=1;
		}
		connected = 0;
		start =0;
		//diagonally
		for(int i =0; i<2;i++){
			if(state[start] == ' ')break;
			if(state[start + 14] == state[start]) connected++;
			start+=14;
		}
		if(connected == 2){
			game->winner = move->move == 'X' ? FIRST_PLAYER_ROLE :SECOND_PLAYER_ROLE;
			game->ended=1;
		}
		connected = 0;
		start =4;
		//counter diagonal
		for(int i =0; i<2;i++){
			if(state[start] == ' ')break;
			if(state[start + 10] == state[start]) connected++;
			start+=10;
		}
		if(connected == 2){
			game->winner = move->move == 'X' ? FIRST_PLAYER_ROLE :SECOND_PLAYER_ROLE;
			game->ended=1;
		}
	}else if(position >= 6 && position <= 8){
		int relative_pos = position - 6;
		if(state[24+relative_pos*2] != ' '){
			debug("can't make a move on taken position\n");
			pthread_mutex_unlock(&game->lock);
			return -1;
		}
		state[24 +relative_pos*2] = move->move;

		int connected = 0;
		int start = 4;
		//vertical
		for(int i =0; i<2;i++){
			if(state[start] == ' ')break;
			if(state[start +12] == state[start])connected++;
			start+=12;
		}
		if(connected == 2){
			game->winner = move->move == 'X' ? FIRST_PLAYER_ROLE :SECOND_PLAYER_ROLE;
			game->ended=1;
		}
		connected = 0;
		start =24;
		//horizontal
		for(int i =0; i<2;i++){
			if(state[start] == ' ')break;
			if(state[start + 2] == state[start]) connected++;
			start+=2;
		}
		if(connected == 2){
			game->winner = move->move == 'X' ? FIRST_PLAYER_ROLE :SECOND_PLAYER_ROLE;
			game->ended=1;
		}
		connected = 0;
		start =0;
		//diagonal
		for(int i =0; i<2;i++){
			if(state[start] == ' ')break;
			if(state[start + 14] == state[start]) connected++;
			start+=14;
		}
		if(connected == 2){
			game->winner = move->move == 'X' ? FIRST_PLAYER_ROLE :SECOND_PLAYER_ROLE;
			game->ended=1;
		}
		connected = 0;
		start =4;
		//counter diagonally
		for(int i =0; i<2;i++){
			if(state[start] == ' ')break;
			if(state[start + 10] == state[start]) connected++;
			start+=10;
		}
		if(connected == 2){
			game->winner = move->move == 'X' ? FIRST_PLAYER_ROLE :SECOND_PLAYER_ROLE;
			game->ended=1;
		}
	}
	state[30] = move->move == 'X' ? 'O' : 'X';
	game->previous_move = move->move;
	game->moves_made++;
	if(game->moves_made == 9 && game->winner == NULL_ROLE){
		game->ended = 1;
	}
	debug("move made\n");
	pthread_mutex_unlock(&game->lock);
	return 0;
}

int game_resign(GAME *game, GAME_ROLE role){
	if(game == NULL){return -1;}
	pthread_mutex_lock(&game->lock);
	if(game->ended){ // game already ended
		pthread_mutex_unlock(&game->lock);
		return -1;
	}
	game->ended = 1;
	game->winner = role == FIRST_PLAYER_ROLE ? SECOND_PLAYER_ROLE : FIRST_PLAYER_ROLE;
	debug("submit game resignation\n");
	pthread_mutex_unlock(&game->lock);
	return 0;
}

char *game_unparse_state(GAME *game){
	if(game == NULL) return NULL;
	pthread_mutex_lock(&game->lock);
	char *state = calloc(1, strlen(game->current_state)+1);
	strncpy(state,game->current_state,strlen(game->current_state));
	pthread_mutex_unlock(&game->lock);
	return state;
}

int game_is_over(GAME *game){
	if(game==NULL) return -1;
	return game->ended;
}

GAME_ROLE game_get_winner(GAME *game){
	if(game == NULL) return NULL_ROLE;
	return game->winner;
}

GAME_MOVE *game_parse_move(GAME *game, GAME_ROLE role, char *str){
	if(game == NULL) return NULL;

	pthread_mutex_lock(&game->lock);
	GAME_MOVE *move = calloc(1,sizeof(GAME_MOVE));
	if(role == FIRST_PLAYER_ROLE){
		// first person is X
		if(strlen(str) == 1){
			char num = *str;
			if(num >= '1' && num <= '9'){
				move->placement = num-48;
				move->move  = 'X';
			}else{
				// not a valid move
				pthread_mutex_unlock(&game->lock);
				free(move);
				return NULL;
			}
		}else if(strlen(str) == 4){
			// n<-X
			char num = *str;
			if((num < '1' || num > '9') || *(str+1) != '<' || *(str+2) != '-' || *(str+3) != 'X'){
				pthread_mutex_unlock(&game->lock);
				free(move);
				return NULL;
			}
			move->placement = num-48;
			move->move = 'X';
		}else{
			// not a valid move
			pthread_mutex_unlock(&game->lock);
			free(move);
			return NULL;
		}
	}else if(role == SECOND_PLAYER_ROLE){
		// second person is O
		if(strlen(str) == 1){
			char num = *str;
			if(num >= '1' && num <= '9'){
				move->placement = num-48;
				move->move  = 'O';
			}else{
				// not a valid move
				pthread_mutex_unlock(&game->lock);
				free(move);
				return NULL;
			}
		}else if(strlen(str) == 4){
			// n<-X;
			char num = *str;
			if((num < '1' || num > '9') || *(str+1) != '<' || *(str+2) != '-' || *(str+3) != 'O'){
				pthread_mutex_unlock(&game->lock);
				free(move);
				return NULL;
			}
			move->placement = num-48;
			move->move = 'O';
			move->move = 'O';
		}else{
			// not a valid move
			pthread_mutex_unlock(&game->lock);
			return NULL;
		}
	}
	pthread_mutex_unlock(&game->lock);
	return move;
}

char *game_unparse_move(GAME_MOVE *move){
	if(move == NULL) return NULL;
	char *un_parsed_move = calloc(1,2);
	un_parsed_move[0] = move->move;
	return un_parsed_move;
}
