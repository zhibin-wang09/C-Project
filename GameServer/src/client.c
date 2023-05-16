#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

#include "debug.h"
#include "jeux_globals.h"
#include "client_registry.h"
#include "client.h"
#include "invitation.h"

struct client{
	int fd;
	PLAYER *player;
	INVITATION *invitation_list[255]; // largest value of a 8 bit unsigned value is 255
	int reference_count;
    pthread_mutexattr_t recursive_type;
	pthread_mutex_t lock;
};

CLIENT *client_create(CLIENT_REGISTRY *creg, int fd){
	CLIENT *client = calloc(1,sizeof(CLIENT));
	client->fd = fd;
    pthread_mutexattr_init(&client->recursive_type);
    pthread_mutexattr_settype(&client->recursive_type,PTHREAD_MUTEX_RECURSIVE); // set the mutex to be recursive in order to do function calls of the same nested mutex
	pthread_mutex_init(&client->lock,&client->recursive_type);
    client->reference_count=0;
    client_ref(client,"newly created client");
    client->player = NULL;
	return client;
}

CLIENT *client_ref(CLIENT *client, char *why){
	if(client == NULL) return NULL;
	pthread_mutex_lock(&client->lock);
	client->reference_count++;
	debug("Increase reference count on client (%d -> %d) %s",client->reference_count-1, client->reference_count,why);
	pthread_mutex_unlock(&client->lock);
	return client;
}

void client_unref(CLIENT *client, char *why){
	if(client == NULL) return;
	pthread_mutex_lock(&client->lock);
	client->reference_count--;
	debug("Decrease reference count on client (%d -> %d) %s",client->reference_count+1, client->reference_count,why);
	if(client->reference_count == 0){ // client and its associated fields should be freed
		client_logout(client);
		for(int i =0; i<255;i++){
			if(client->invitation_list[i] != NULL) inv_unref(client->invitation_list[i],"client is freed");
		}
        if(client->player != NULL) player_unref(client->player,"client is freed");
		pthread_mutex_unlock(&client->lock);
		pthread_mutex_destroy(&client->lock);
        pthread_mutexattr_destroy(&client->recursive_type);
		free(client);
		return;
	}
	pthread_mutex_unlock(&client->lock);
}

int client_login(CLIENT *client, PLAYER *player){
	if(client == NULL || player == NULL){debug("argument not valid in login"); return -1;}
	pthread_mutex_lock(&client->lock);
	if(client->player){debug("already logged in"); pthread_mutex_unlock(&client->lock); return -1; }// already logged in
    CLIENT *exist = creg_lookup(client_registry,player_get_name(player)); // look up if the player with the specific user name exist already can not have repeated username
    if(exist != NULL){client_unref(exist,"found client exist therefore discard reference"); pthread_mutex_unlock(&client->lock); return -1;}
	client->player = player;
	player_ref(player,"logged in, reference retained by client");
	pthread_mutex_unlock(&client->lock);
	return 0;
}

int client_logout(CLIENT *client){
	if(client == NULL) return -1;
    if(client->player == NULL) return -1; // if client is not logged it's error
	pthread_mutex_lock(&client->lock);
    // INVITATION needs to be revoked or declined, any gams in progress are resigend
    // invitations are removed from the list of this client and its opponents
    for(int i =0;i<255;i++){
        INVITATION *inv = client->invitation_list[i];
        if(inv != NULL){
            //decline or revoke then remove from this client list and its opponent
            if(client_resign_game(client,i) == 0) continue; // resign if possible
            if(inv_get_source(inv) == client){ // find the role of the client, if source then revoke all outstanding invitiation
                client_revoke_invitation(client,i);
            }else{// if target then decline all outstanding invitation
                client_decline_invitation(client,i);
            }
        }
    }
	player_unref(client->player,"player logged out...");
	client->player = NULL;
	pthread_mutex_unlock(&client->lock);
	return 0;
}

PLAYER *client_get_player(CLIENT *client){
	if(client == NULL) return NULL;
	return client->player;
}

int client_get_fd(CLIENT *client){
	if(client == NULL) return -1;
	return client->fd;
}

int client_send_packet(CLIENT *player, JEUX_PACKET_HEADER *pkt, void *data){
	if(player == NULL) return -1;
	pthread_mutex_lock(&player->lock);
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC,&time);
    pkt->timestamp_sec = htonl(time.tv_sec);
    pkt->timestamp_nsec = htonl(time.tv_nsec);
	if(proto_send_packet(player->fd,pkt,data)){ // failed to send packet
        pthread_mutex_unlock(&player->lock);
        debug("failed to send packet");
        return -1;
    }
    debug("=> %u.%u: type=%d, size=%d, id=%d, role=%d paylopd=%s",ntohl(pkt->timestamp_sec),ntohl(pkt->timestamp_nsec),pkt->type,ntohs(pkt->size),pkt->id,pkt->role,(char *)data);
    debug("client sent packet");
	pthread_mutex_unlock(&player->lock);
	return 0;
}

int client_send_ack(CLIENT *client, void *data, size_t datalen){
	if(client == NULL) return -1;
	pthread_mutex_lock(&client->lock);
	JEUX_PACKET_HEADER header = {0};
	header.type = JEUX_ACK_PKT;
	header.size = htons(datalen);
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC,&time);
    header.timestamp_sec = htonl(time.tv_sec);
    header.timestamp_nsec = htonl(time.tv_nsec);
	if(proto_send_packet(client->fd,&header,data)){
        pthread_mutex_unlock(&client->lock);
        debug("failed to send ack");
        return -1;
    }
    debug("=> %u.%u: type=%d, size=%d, id=%d, role=%d paylopd=%s",ntohl(header.timestamp_sec),ntohl(header.timestamp_nsec),header.type,ntohs(header.size),header.id,header.role,(char *)data);
    debug("client sent ack");
	pthread_mutex_unlock(&client->lock);
	return 0;
}

int client_send_nack(CLIENT *client){
	if(client == NULL) return -1;
	pthread_mutex_lock(&client->lock);
	JEUX_PACKET_HEADER header = {0};
	header.type = JEUX_NACK_PKT;
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC,&time);
    header.timestamp_sec = htonl(time.tv_sec);
    header.timestamp_nsec = htonl(time.tv_nsec);
	if(proto_send_packet(client->fd,&header,NULL)){
        pthread_mutex_unlock(&client->lock);
        debug("failed to send nack");
        return -1;
    }
    debug("=> %u.%u: type=%d, size=%d, id=%d, role=%d",ntohl(header.timestamp_sec),ntohl(header.timestamp_nsec),header.type,ntohs(header.size),header.id,header.role);
    debug("send nack");
	pthread_mutex_unlock(&client->lock);
	return 0;
}

int client_add_invitation(CLIENT *client, INVITATION *inv){
	if(client == NULL || inv == NULL) return -1;
	pthread_mutex_lock(&client->lock);
	int i;
	for(i =0; i<255;i++){
		if(client->invitation_list[i] == NULL)break;
	}
	client->invitation_list[i] = inv; // invitation id is the index of the invitation
	inv_ref(inv,"reference retained by client's invitation list");
	pthread_mutex_unlock(&client->lock);

	return i;
}

int client_remove_invitation(CLIENT *client, INVITATION *inv){
	if(client == NULL || inv == NULL) return -1;
	pthread_mutex_lock(&client->lock);
	int i;
	for(i =0; i<255;i++){
		if(client->invitation_list[i] == inv)break;
	}
    if(i == 255){ // invitation not foudn in the list
        pthread_mutex_unlock(&client->lock);
        debug("invitation was not found in the attempt to remove invitation");
        return -1;
    }
	client->invitation_list[i] = NULL;
	inv_unref(inv,"reference released by client after removing invitation");
	pthread_mutex_unlock(&client->lock);

	return 0;
}

int client_make_invitation(CLIENT *source, CLIENT *target,
			   GAME_ROLE source_role, GAME_ROLE target_role){
	if(source == NULL || target == NULL) return -1;
	INVITATION *inv = inv_create(source,target,source_role,target_role);
	if(inv == NULL) return -1; // invitation create has error
	pthread_mutex_lock(&source->lock);
	pthread_mutex_lock(&target->lock);
	int source_id = client_add_invitation(source,inv); // add invitation to the source invitation list
	int target_id = client_add_invitation(target,inv); // add invitation to the target invitation list
	JEUX_PACKET_HEADER hdr = {0};
	hdr.id = target_id;
	hdr.type = JEUX_INVITED_PKT;
    hdr.size = htons(strlen(player_get_name(client_get_player(source)))+1);
    hdr.role = target_role;
	if(client_send_packet(target,&hdr,player_get_name(client_get_player(source)))){ // send INVITED PACKET to target
        pthread_mutex_unlock(&source->lock);
        pthread_mutex_unlock(&target->lock);
        debug("client send INVITED packet failed");
        return -1;
    }
    debug("client send INVITED packet succeed");
    inv_unref(inv,"reference released after adding invitation to client's list");
	pthread_mutex_unlock(&source->lock);
	pthread_mutex_unlock(&target->lock);
	return source_id;
}

int client_revoke_invitation(CLIENT *client, int id){
    if(client == NULL) return -1;
    pthread_mutex_lock(&client->lock);
    INVITATION *inv = client->invitation_list[id]; // get the invitation object
    if(inv == NULL || inv_get_source(inv) != client){ // invitation does not exist or client argument is not soruce
        pthread_mutex_unlock(&client->lock);
        return -1;
    }

    if(inv_close(inv,NULL_ROLE)){ // game role is 0 because no game is in progress
         // inv_close did not work
        pthread_mutex_unlock(&client->lock);
        return -1;
    }

    CLIENT *target = inv_get_target(inv);
    JEUX_PACKET_HEADER hdr = {0}; // revoke packet
    hdr.type = JEUX_REVOKED_PKT;
    int target_id = -1;
    for(int i =0;i<255;i++){
        if(target->invitation_list[i] == inv){
            target_id = i;
            break;
        }
    }
    hdr.id = target_id;
    client_send_packet(target,&hdr,NULL); // send packet

    if(client_remove_invitation(target,inv)){ // invitation is removed from the target client list if successful
        // invitation is not found in target's invitation list
        pthread_mutex_unlock(&client->lock);
        return -1;
    }

    if(client_remove_invitation(client,inv)){ // invitation is removed from the source client list if successful
        // invitation is not found in source's invitation list
        pthread_mutex_unlock(&client->lock);
        return -1;
    }
    debug("revoked packet sent");
    pthread_mutex_unlock(&client->lock);

    return 0;
}

int client_decline_invitation(CLIENT *client, int id){
    if(client == NULL) return -1;
    pthread_mutex_lock(&client->lock);
    INVITATION *inv = client->invitation_list[id];
    if(inv == NULL || inv_get_target(inv) != client){ // invitation does not exist or client argument is not target
        pthread_mutex_unlock(&client->lock);
        return -1;
    }

    if(inv_close(inv,NULL_ROLE)){ // game role is 0 because no game is in progress
         // inv_close did not work
        pthread_mutex_unlock(&client->lock);
        return -1;
    }

    CLIENT *source = inv_get_source(inv);
    JEUX_PACKET_HEADER hdr = {0}; // decline packet
    hdr.type = JEUX_DECLINED_PKT;
    int source_id = -1;
    for(int i =0;i<255;i++){
        if(source->invitation_list[i] == inv){
            source_id = i;
            break;
        }
    }
    hdr.id = source_id;
    client_send_packet(source,&hdr,NULL); // send packet

    if(client_remove_invitation(source,inv)){ // invitation is removed from the target client list if successful
        // invitation is not found in source's invitation list
        pthread_mutex_unlock(&client->lock);
        return -1;
    }

    if(client_remove_invitation(client,inv)){ // invitation is removed from the target client list if successful
        // invitation is not found in target's invitation list
        pthread_mutex_unlock(&client->lock);
        return -1;
    }
    debug("declined packet sent");
    pthread_mutex_unlock(&client->lock);
    return 0;
}

int client_accept_invitation(CLIENT *client, int id, char **strp){
    if(client == NULL) return -1;
    pthread_mutex_lock(&client->lock);
    INVITATION *inv = client->invitation_list[id];
    if(inv == NULL || inv_get_target(inv) != client){
        // invitation does not exist or the CLIENT is not the target
        pthread_mutex_unlock(&client->lock);
        return -1;
    }
    int source_id = -1;
    CLIENT *source = inv_get_source(inv);
    client_ref(source,"reference obtained temporarily to search through invitation");
    for(int i =0; i<255;i++){ // finding the id of the source
        if(source->invitation_list[i] == inv){
            source_id = i;
            break;
        }
    }
    if(source_id == -1){ // invitation in source not found
        client_unref(source,"reference released after failed in searching for source id");
        pthread_mutex_unlock(&client->lock);
        return -1;
    }
    if(inv_accept(inv)){
        // accepting invitation resulted in error
        pthread_mutex_unlock(&client->lock);
        return -1;
    }
    GAME *game = inv_get_game(inv);
    game_ref(game,"reference obtained by client_accept_invitation() temporarily");
    int source_role = inv_get_source_role(inv);
    JEUX_PACKET_HEADER hdr= {0};
    hdr.type = JEUX_ACCEPTED_PKT;
    hdr.id = source_id;
    // send packet to source
    if(source_role == FIRST_PLAYER_ROLE){
        // send accept packet to source with payload
        char *initial_state = game_unparse_state(game);
        hdr.size = htons(strlen(initial_state)+1);
        client_send_packet(source,&hdr,initial_state);
        client_unref(source,"reference released after searching for source id");
        game_unref(game,"reference released after obtaining the initail game state");
        free(initial_state);
        *strp= NULL;
    }
    if(source_role == SECOND_PLAYER_ROLE){
        // send accept packet to source with no payload
        client_send_packet(source,&hdr,NULL);
        client_unref(source,"reference released after searching for source id");
        char *initial_state = game_unparse_state(game);
        *strp = initial_state;
        game_unref(game,"reference released after obtaining the initail game state");
    }
    pthread_mutex_unlock(&client->lock);
    return 0;
}

int client_resign_game(CLIENT *client, int id){
    if(client == NULL) return -1;

    pthread_mutex_lock(&client->lock);
    INVITATION *inv = client->invitation_list[id];
    if(inv==NULL){ // invitation does not exist;
        pthread_mutex_unlock(&client->lock);
        return -1;
    }
    CLIENT *opp;
    if(inv_get_source(inv) == client){ // client is source
        GAME_ROLE source_role = inv_get_source_role(inv);
        if(inv_close(inv,source_role)){ // closing resulted in error
            pthread_mutex_unlock(&client->lock);
            return -1;
        }
        opp = inv_get_target(inv);
        client_ref(opp,"client_resign_game() get the oppponent's reference");
        JEUX_PACKET_HEADER hdr = {0};
        hdr.type = JEUX_RESIGNED_PKT;
        int opp_id = -1;
        for(int i=0;i<255;i++){ // finding the opponent id
            if(opp->invitation_list[i] == inv) {opp_id = i; break;}
        }
        hdr.id = opp_id;
        client_send_packet(opp,&hdr,NULL);
    }

    if(inv_get_target(inv) == client){ // client is target
        GAME_ROLE target_role = inv_get_target_role(inv);
        if(inv_close(inv,target_role)){ // closing resulted in error
            debug("resign failed");
            pthread_mutex_unlock(&client->lock);
            return -1;
        }
        opp = inv_get_source(inv);
        client_ref(opp,"get the oppponent's reference");
        JEUX_PACKET_HEADER hdr = {0};
        hdr.type = JEUX_RESIGNED_PKT;
        int opp_id = -1;
        for(int i=0;i<255;i++){ // finding the opponent id
            if(opp->invitation_list[i] == inv) {opp_id = i; break;}
        }
        hdr.id = opp_id;
        client_send_packet(opp,&hdr,NULL);
    }

    GAME *game = inv_get_game(inv);
    game_ref(game,"reference obtained by client_resign_game() temporarily to check for game state");
    if(game_is_over(game)){// game is resigned
       GAME_ROLE winner = game_get_winner(game);
       CLIENT *source = inv_get_source(inv);
       CLIENT *target = inv_get_target(inv);
       client_ref(source,"reference obtained by client_resign_game() to get player and update rating");
       client_ref(target,"reference obtained by client_resign_game() to get player and update rating");
       CLIENT *opp;
       if(inv_get_source(inv) == client){
            opp=inv_get_target(inv);
       }else{
            opp=inv_get_source(inv);
       }
       player_post_result(client_get_player(client),client_get_player(opp),SECOND_PLAYER_ROLE);

       JEUX_PACKET_HEADER ended_packet_1 = {0};
       ended_packet_1.type = JEUX_ENDED_PKT;
       ended_packet_1.role = winner;

       int ended_packet_1_id = -1;
       for(int i =0;i<255;i++){
            if(source->invitation_list[i] == inv){
                ended_packet_1_id = i;
                break;
            }
       }
       ended_packet_1.id = ended_packet_1_id;

       client_send_packet(source,&ended_packet_1,NULL); // send ended packet to resigned player
       JEUX_PACKET_HEADER ended_packet_2 = {0};
       ended_packet_2.type = JEUX_ENDED_PKT;
       ended_packet_2.role = winner;

       int ended_packet_2_id = -1;
       for(int i =0;i<255;i++){
            if(target->invitation_list[i] == inv){
                ended_packet_2_id = i;
                break;
            }
       }
       ended_packet_2.id = ended_packet_2_id;
       client_send_packet(target,&ended_packet_2,NULL);
       client_unref(source,"source reference released after updating rating");
       client_unref(target,"target reference released after updating rating");
    }

    client_remove_invitation(client,inv);
    client_remove_invitation(opp,inv); // remove the invitation from the opponent's list as well
    client_unref(opp,"reference released because packet sent to opponent");
    game_unref(game,"reference released by client_resign_game()");
    pthread_mutex_unlock(&client->lock);
    return 0;
}

int client_make_move(CLIENT *client, int id, char *move){
    if(client == NULL) return -1;
    int opp_side=-1; // used to determine if the client is source or target
    pthread_mutex_lock(&client->lock);
    INVITATION *inv = client->invitation_list[id];
    inv_ref(inv,"reference used to locate the invitation to make a move");
    if(inv == NULL){
        //invitation does not exist
        pthread_mutex_unlock(&client->lock);
        return -1;
    }
    GAME_ROLE client_role;
    if(inv_get_source(inv) == client){ // if the client is source
        client_role = inv_get_source_role(inv);
        opp_side = 1;
    }
    if(inv_get_target(inv) == client){ // if the client is target
        client_role = inv_get_target_role(inv);
        opp_side = 0;
    }
    GAME *game = inv_get_game(inv);
    game_ref(game,"reference used to make move on the game");
    GAME_MOVE *game_move = game_parse_move(game,client_role,move);
    if(game_move == NULL){ // if parsing game move resulted in failure
        debug("parsing game move failed");
        inv_unref(inv,"done using the reference");
        game_unref(game,"done using the reference");
        pthread_mutex_unlock(&client->lock);
        return -1;
    }
    if(game_apply_move(game,game_move)){
        // applying thr game move resulted in failure
        debug("applying game move failed");
        inv_unref(inv,"done using the reference");
        game_unref(game,"done using the reference");
        free(game_move);
        pthread_mutex_unlock(&client->lock);
        return -1;
    }
    free(game_move); // need to free GAME_MOVE after apply
    //everything worked now send packet
    JEUX_PACKET_HEADER hdr = {0};
    hdr.type = JEUX_MOVED_PKT;
    char *current_state = game_unparse_state(game);
    hdr.size = htons(strlen(current_state)+1);
    int client_id = -1;
    if(opp_side == 0){
        // send moved pkt to source
        CLIENT *source = inv_get_source(inv);
        for(int i =0;i<255;i++){
            if(source->invitation_list[i] == inv){client_id = i;break;}
        }
        hdr.id = client_id;
        client_send_packet(source,&hdr,current_state);
    }else if(opp_side == 1){
        //send moved pkt to target
        CLIENT *target = inv_get_target(inv);
        for(int i =0;i<255;i++){
            if(target->invitation_list[i] == inv){client_id = i;break;}
        }
        hdr.id = client_id;
        client_send_packet(target,&hdr,current_state);
    }
    free(current_state);

    if(game_is_over(game)){// game is over
       GAME_ROLE winner = game_get_winner(game);
       CLIENT *source = inv_get_source(inv);
       CLIENT *target = inv_get_target(inv);
       client_ref(source,"reference obtained by client_resign_game to get player and update rating");
       client_ref(target,"reference obtained by client_resign_game to get player and update rating");
       if(winner == FIRST_PLAYER_ROLE){
            if(inv_get_source_role(inv) == FIRST_PLAYER_ROLE){
                player_post_result(client_get_player(source),client_get_player(target),winner);
            }else if(inv_get_source_role(inv) == SECOND_PLAYER_ROLE){
                player_post_result(client_get_player(target),client_get_player(source),winner);
            }
       }else if(winner == SECOND_PLAYER_ROLE){
            if(inv_get_source_role(inv) == SECOND_PLAYER_ROLE){
                player_post_result(client_get_player(target),client_get_player(source),winner);
            }else if(inv_get_source_role(inv) == FIRST_PLAYER_ROLE){
                player_post_result(client_get_player(source),client_get_player(target),winner);
            }
       }

       JEUX_PACKET_HEADER ended_packet_1 = {0};
       ended_packet_1.type = JEUX_ENDED_PKT;
       ended_packet_1.role = winner;

       int ended_packet_1_id = -1;
       for(int i =0;i<255;i++){
            if(source->invitation_list[i] == inv){
                ended_packet_1_id = i;
                break;
            }
       }
       ended_packet_1.id = ended_packet_1_id;

       client_send_packet(source,&ended_packet_1,NULL); // send ended packet to resigned player
       JEUX_PACKET_HEADER ended_packet_2 = {0};
       ended_packet_2.type = JEUX_ENDED_PKT;
       ended_packet_2.role = winner;

       int ended_packet_2_id = -1;
       for(int i =0;i<255;i++){
            if(target->invitation_list[i] == inv){
                ended_packet_2_id = i;
                break;
            }
       }
       ended_packet_2.id = ended_packet_2_id;
       client_send_packet(target,&ended_packet_2,NULL);
       client_remove_invitation(source,inv);
       client_remove_invitation(target,inv);
       client_unref(source,"source reference released after updating rating");
       client_unref(target,"target reference released after updating rating");
    }
    inv_unref(inv,"done using the reference");
    game_unref(game,"done using the reference");
    pthread_mutex_unlock(&client->lock);
    return 0;
}
