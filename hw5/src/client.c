#include <stdlib.h>
#include <pthread.h>
#include <debug.h>
#include <string.h>

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

/*
 * Create a new CLIENT object with a specified file descriptor with which
 * to communicate with the client.  The returned CLIENT has a reference
 * count of one and is in the logged-out state.
 *
 * @param creg  The client registry in which to create the client.
 * @param fd  File descriptor of a socket to be used for communicating
 * with the client.
 * @return  The newly created CLIENT object, if creation is successful,
 * otherwise NULL.
 */
CLIENT *client_create(CLIENT_REGISTRY *creg, int fd){
	CLIENT *client = calloc(1,sizeof(CLIENT));
	client->fd = fd;
    pthread_mutexattr_init(&client->recursive_type);
    pthread_mutexattr_settype(&client->recursive_type,PTHREAD_MUTEX_RECURSIVE); // set the mutex to be recursive in order to do function calls of the same nested mutex
	pthread_mutex_init(&client->lock,&client->recursive_type);
    client->reference_count=0;
	return client;
}

/*
 * Increase the reference count on a CLIENT by one.
 *
 * @param client  The CLIENT whose reference count is to be increased.
 * @param why  A string describing the reason why the reference count is
 * being increased.  This is used for debugging printout, to help trace
 * the reference counting.
 * @return  The same CLIENT that was passed as a parameter.
 */
CLIENT *client_ref(CLIENT *client, char *why){
	if(client == NULL) return NULL;
	pthread_mutex_lock(&client->lock);
	client->reference_count++;
	debug("%s",why);
	pthread_mutex_unlock(&client->lock);
	return client;
}

/*
 * Decrease the reference count on a CLIENT by one.  If after
 * decrementing, the reference count has reached zero, then the CLIENT
 * and its contents are freed.
 *
 * @param client  The CLIENT whose reference count is to be decreased.
 * @param why  A string describing the reason why the reference count is
 * being decreased.  This is used for debugging printout, to help trace
 * the reference counting.
 */
void client_unref(CLIENT *client, char *why){
	if(client == NULL) return;
	pthread_mutex_lock(&client->lock);
	client->reference_count--;
	debug("%s",why);
	if(client->reference_count == 0){ // client should be freed
		client_logout(client);
		for(int i =0; i<255;i++){
			if(client->invitation_list[i] != NULL) inv_unref(client->invitation_list[i],"client is freed\n");
		}
		pthread_mutex_unlock(&client->lock);
		pthread_mutex_destroy(&client->lock);
        pthread_mutexattr_destroy(&client->recursive_type);
		free(client);
		return;
	}
	pthread_mutex_unlock(&client->lock);
}

/*
 * Log in this CLIENT as a specified PLAYER.
 * The login fails if the CLIENT is already logged in or there is already
 * some other CLIENT that is logged in as the specified PLAYER.
 * Otherwise, the login is successful, the CLIENT is marked as "logged in"
 * and a reference to the PLAYER is retained by it.  In this case,
 * the reference count of the PLAYER is incremented to account for the
 * retained reference.
 *
 * @param CLIENT  The CLIENT that is to be logged in.
 * @param PLAYER  The PLAYER that the CLIENT is to be logged in as.
 * @return 0 if the login operation is successful, otherwise -1.
 */
int client_login(CLIENT *client, PLAYER *player){
	if(client == NULL || player == NULL) return -1;
	pthread_mutex_lock(&client->lock);
	if(!client->player) return -1; // already logged in
	client->player = player;
	player_ref(player,"logged in, reference retained by client");
	pthread_mutex_unlock(&client->lock);
	return 0;
}

/*
 * Log out this CLIENT.  If the client was not logged in, then it is
 * an error.  The reference to the PLAYER that the CLIENT was logged
 * in as is discarded, and its reference count is decremented.  Any
 * INVITATIONs in the client's list are revoked or declined, if
 * possible, any games in progress are resigned, and the invitations
 * are removed from the list of this CLIENT as well as its opponents'.
 *
 * @param client  The CLIENT that is to be logged out.
 * @return 0 if the client was logged in and has been successfully
 * logged out, otherwise -1.
 */
int client_logout(CLIENT *client){
	if(client == NULL) return -1;
	pthread_mutex_lock(&client->lock);
	player_unref(client->player,"player logged out...");
	client->player = NULL;
	pthread_mutex_unlock(&client->lock);
	return 0;
}

/*
 * Get the PLAYER for the specified logged-in CLIENT.
 * The reference count on the returned PLAYER is NOT incremented,
 * so the returned reference should only be regarded as valid as long
 * as the CLIENT has not been freed.
 *
 * @param client  The CLIENT from which to get the PLAYER.
 * @return  The PLAYER that the CLIENT is currently logged in as,
 * otherwise NULL if the player is not currently logged in.
 */
PLAYER *client_get_player(CLIENT *client){
	if(client == NULL) return NULL;
	return client->player;
}

/*
 * Get the file descriptor for the network connection associated with
 * this CLIENT.
 *
 * @param client  The CLIENT for which the file descriptor is to be
 * obtained.
 * @return the file descriptor.
 */
int client_get_fd(CLIENT *client){
	if(client == NULL) return -1;
	return client->fd;
}

/*
 * Send a packet to a client.  Exclusive access to the network connection
 * is obtained for the duration of this operation, to prevent concurrent
 * invocations from corrupting each other's transmissions.  To prevent
 * such interference, only this function should be used to send packets to
 * the client, rather than the lower-level proto_send_packet() function.
 *
 * @param client  The CLIENT who should be sent the packet.
 * @param pkt  The header of the packet to be sent.
 * @param data  Data payload to be sent, or NULL if none.
 * @return 0 if transmission succeeds, -1 otherwise.
 */
int client_send_packet(CLIENT *player, JEUX_PACKET_HEADER *pkt, void *data){
	if(player == NULL) return -1;
	pthread_mutex_lock(&player->lock);
	if(proto_send_packet(player->fd,pkt,data)){ // failed to send packet
        pthread_mutex_unlock(&player->lock);
        return -1;
    }
	pthread_mutex_unlock(&player->lock);
	return 0;
}

/*
 * Send an ACK packet to a client.  This is a convenience function that
 * streamlines a common case.
 *
 * @param client  The CLIENT who should be sent the packet.
 * @param data  Pointer to the optional data payload for this packet,
 * or NULL if there is to be no payload.
 * @param datalen  Length of the data payload, or 0 if there is none.
 * @return 0 if transmission succeeds, -1 otherwise.
 */
int client_send_ack(CLIENT *client, void *data, size_t datalen){
	if(client == NULL) return -1;
	pthread_mutex_lock(&client->lock);
	JEUX_PACKET_HEADER header = {0};
	header.type = JEUX_ACK_PKT;
	header.size = datalen;
	if(proto_send_packet(client->fd,&header,data)){
        pthread_mutex_unlock(&client->lock);
        return -1;
    }
	pthread_mutex_unlock(&client->lock);
	return 0;
}

/*
 * Send an NACK packet to a client.  This is a convenience function that
 * streamlines a common case.
 *
 * @param client  The CLIENT who should be sent the packet.
 * @return 0 if transmission succeeds, -1 otherwise.
 */
int client_send_nack(CLIENT *client){
	if(client == NULL) return -1;
	pthread_mutex_lock(&client->lock);
	JEUX_PACKET_HEADER header = {0};
	header.type = JEUX_NACK_PKT;
	if(proto_send_packet(client->fd,&header,NULL)){
        pthread_mutex_unlock(&client->lock);
        return -1;
    }
	pthread_mutex_unlock(&client->lock);
	return 0;
}

/*
 * Add an INVITATION to the list of outstanding invitations for a
 * specified CLIENT.  A reference to the INVITATION is retained by
 * the CLIENT and the reference count of the INVITATION is
 * incremented.  The invitation is assigned an integer ID,
 * which the client subsequently uses to identify the invitation.
 *
 * @param client  The CLIENT to which the invitation is to be added.
 * @param inv  The INVITATION that is to be added.
 * @return  The ID assigned to the invitation, if the invitation
 * was successfully added, otherwise -1.
 */
int client_add_invitation(CLIENT *client, INVITATION *inv){
	if(client == NULL || inv == NULL) return -1;
	pthread_mutex_lock(&client->lock);
	int i;
	for(i =0; i<255;i++){
		if(client->invitation_list[i] == NULL)break;
	}
	client->invitation_list[i] = inv;
	inv_ref(inv,"reference retained by client\n");
	pthread_mutex_unlock(&client->lock);

	return i;
}

/*
 * Remove an invitation from the list of outstanding invitations
 * for a specified CLIENT.  The reference count of the invitation is
 * decremented to account for the discarded reference.
 *
 * @param client  The client from which the invitation is to be removed.
 * @param inv  The invitation that is to be removed.
 * @return the CLIENT's id for the INVITATION, if it was successfully
 * removed, otherwise -1.
 */
int client_remove_invitation(CLIENT *client, INVITATION *inv){
	if(client == NULL || inv == NULL) return -1;
	pthread_mutex_lock(&client->lock);
	int i;
	for(i =0; i<255;i++){
		if(client->invitation_list[i] == inv)break;
	}
    if(i == 255){ // invitation not foudn in the list
        pthread_mutex_unlock(&client->lock);
        return -1;
    }
	client->invitation_list[i] = NULL;
	inv_unref(inv,"reference released by client after removing invitation\n");
	pthread_mutex_unlock(&client->lock);

	return 0;
}

/*
 * Make a new invitation from a specified "source" CLIENT to a specified
 * target CLIENT.  The invitation represents an offer to the target to
 * engage in a game with the source.  The invitation is added to both the
 * source's list of invitations and the target's list of invitations and
 * the invitation's reference count is appropriately increased.
 * An `INVITED` packet is sent to the target of the invitation.
 *
 * @param source  The CLIENT that is the source of the INVITATION.
 * @param target  The CLIENT that is the target of the INVITATION.
 * @param source_role  The GAME_ROLE to be played by the source of the INVITATION.
 * @param target_role  The GAME_ROLE to be played by the target of the INVITATION.
 * @return the ID assigned by the source to the INVITATION, if the operation
 * is successful, otherwise -1.
 */
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
	if(client_send_packet(target,&hdr,NULL)){ // send INVITED PACKET to target
        pthread_mutex_unlock(&source->lock);
        pthread_mutex_unlock(&target->lock);
        return -1;
    }
	pthread_mutex_unlock(&source->lock);
	pthread_mutex_unlock(&target->lock);
	return source_id;
}

/*
 * Revoke an invitation for which the specified CLIENT is the source.
 * The invitation is removed from the lists of invitations of its source
 * and target CLIENT's and the reference counts are appropriately
 * decreased.  It is an error if the specified CLIENT is not the source
 * of the INVITATION, or the INVITATION does not exist in the source or
 * target CLIENT's list.  It is also an error if the INVITATION being
 * revoked is in a state other than the "open" state.  If the invitation
 * is successfully revoked, then the target is sent a REVOKED packet
 * containing the target's ID of the revoked invitation.
 *
 * @param client  The CLIENT that is the source of the invitation to be
 * revoked.
 * @param id  The ID assigned by the CLIENT to the invitation to be
 * revoked.
 * @return 0 if the invitation is successfully revoked, otherwise -1.
 */
int client_revoke_invitation(CLIENT *client, int id){
    if(client == NULL) return -1;
    pthread_mutex_lock(&client->lock);
    INVITATION *inv = client->invitation_list[id]; // get the invitation object
    if(inv == NULL || inv_get_source(inv) != client){ // invitation does not exist or client argument is not soruce
        pthread_mutex_unlock(&client->lock);
        return -1;
    }

    CLIENT *target = inv_get_target(inv);
    if(client_remove_invitation(target,inv)){ // invitation is removed from the target client list if successful
        // invitation is not found in target's invitation list
        pthread_mutex_unlock(&client->lock);
        return -1;
    }

    if(client_remove_invitation(client,inv)){ // invitation is removed from the target client list if successful
        // invitation is not found in target's invitation list
        pthread_mutex_unlock(&client->lock);
        return -1;
    }

    if(inv_close(inv,NULL_ROLE)){ // game role is 0 because no game is in progress
         // inv_close did not work
        pthread_mutex_unlock(&client->lock);
        return -1;
    }
    JEUX_PACKET_HEADER hdr = {0}; // revoke packet
    hdr.type = JEUX_REVOKE_PKT;
    hdr.id = id;
    client_send_packet(client,&hdr,NULL); // send packet
    pthread_mutex_unlock(&client->lock);

    return 0;
}

/*
 * Decline an invitation previously made with the specified CLIENT as target.
 * The invitation is removed from the lists of invitations of its source
 * and target CLIENT's and the reference counts are appropriately
 * decreased.  It is an error if the specified CLIENT is not the target
 * of the INVITATION, or the INVITATION does not exist in the source or
 * target CLIENT's list.  It is also an error if the INVITATION being
 * declined is in a state other than the "open" state.  If the invitation
 * is successfully declined, then the source is sent a DECLINED packet
 * containing the source's ID of the declined invitation.
 *
 * @param client  The CLIENT that is the target of the invitation to be
 * declined.
 * @param id  The ID assigned by the CLIENT to the invitation to be
 * declined.
 * @return 0 if the invitation is successfully declined, otherwise -1.
 */
int client_decline_invitation(CLIENT *client, int id){
    if(client == NULL) return -1;
    pthread_mutex_lock(&client->lock);
    INVITATION *inv = client->invitation_list[id];
    if(inv == NULL || inv_get_target(inv) != client){ // invitation does not exist or client argument is not soruce
        pthread_mutex_unlock(&client->lock);
        return -1;
    }
    CLIENT *source = inv_get_source(inv);
    if(client_remove_invitation(source,inv)){ // invitation is removed from the target client list if successful
        // invitation is not found in target's invitation list
        pthread_mutex_unlock(&client->lock);
        return -1;
    }

    if(client_remove_invitation(client,inv)){ // invitation is removed from the target client list if successful
        // invitation is not found in target's invitation list
        pthread_mutex_unlock(&client->lock);
        return -1;
    }

    if(inv_close(inv,NULL_ROLE)){ // game role is 0 because no game is in progress
         // inv_close did not work
        pthread_mutex_unlock(&client->lock);
        return -1;
    }
    JEUX_PACKET_HEADER hdr = {0}; // revoke packet
    hdr.type = JEUX_REVOKE_PKT;
    hdr.id = id;
    client_send_packet(client,&hdr,NULL); // send packet
    pthread_mutex_unlock(&client->lock);
    return 0;
}

/*
 * Accept an INVITATION previously made with the specified CLIENT as
 * the target.  A new GAME is created and a reference to it is saved
 * in the INVITATION.  If the invitation is successfully accepted,
 * the source is sent an ACCEPTED packet containing the source's ID
 * of the accepted INVITATION.  If the source is to play the role of
 * the first player, then the payload of the ACCEPTED packet contains
 * a string describing the initial game state.  A reference to the
 * new GAME (with its reference count incremented) is returned to the
 * caller.
 *
 * @param client  The CLIENT that is the target of the INVITATION to be
 * accepted.
 * @param id  The ID assigned by the target to the INVITATION.
 * @param strp  Pointer to a variable into which will be stored either
 * NULL, if the accepting client is not the first player to move,
 * or a malloc'ed string that describes the initial game state,
 * if the accepting client is the first player to move.
 * If non-NULL, this string should be used as the payload of the `ACK`
 * message to be sent to the accepting client.  The caller must free
 * the string after use.
 * @return 0 if the INVITATION is successfully accepted, otherwise -1.
 */
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
    client_ref(source,"reference obtained temporarily to search through invitation\n");
    for(int i =0; i<255;i++){ // finding the id of the source
        if(client->invitation_list[i] == inv){
            source_id = i;
            break;
        }
    }
    if(source_id == -1){ // invitation in source not found
        client_unref(source,"reference released after failed in searching for source id\n");
        pthread_mutex_unlock(&client->lock);
        return -1;
    }
    if(inv_accept(inv)){
        // accepting invitation resulted in error
        pthread_mutex_unlock(&client->lock);
        return -1;
    }
    GAME *game = inv_get_game(inv);
    game_ref(game,"reference obtained by client accept temporarily\n");
    int source_role = inv_get_source_role(inv);
    JEUX_PACKET_HEADER hdr= {0};
    hdr.type = JEUX_ACCEPTED_PKT;
    hdr.id = source_id;
    // send packet to source
    if(source_role == FIRST_PLAYER_ROLE){
        // send accept packet to source with payload
        char *initial_state = game_unparse_state(game);
        hdr.size = strlen(initial_state)+1;
        client_send_packet(source,&hdr,initial_state);
        client_unref(source,"reference released after searching for source id\n");
        game_unref(game,"reference released after obtaining the initail game state");
    }
    if(source_role == SECOND_PLAYER_ROLE){
        // send accept packet to source with no payload
        client_send_packet(source,&hdr,NULL);
        client_unref(source,"reference released after searching for source id\n");
        char *initial_state = game_unparse_state(game);
        *strp = initial_state;
    }
    pthread_mutex_unlock(&client->lock);
    return 0;
}

/*
 * Resign a game in progress.  This function may be called by a CLIENT
 * that is either source or the target of the INVITATION containing the
 * GAME that is to be resigned.  It is an error if the INVITATION containing
 * the GAME is not in the ACCEPTED state.  If the game is successfully
 * resigned, the INVITATION is set to the CLOSED state, it is removed
 * from the lists of both the source and target, and a RESIGNED packet
 * containing the opponent's ID for the INVITATION is sent to the opponent
 * of the CLIENT that has resigned.
 *
 * @param client  The CLIENT that is resigning.
 * @param id  The ID assigned by the CLIENT to the INVITATION that contains
 * the GAME to be resigned.
 * @return 0 if the game is successfully resigned, otherwise -1.
 */
int client_resign_game(CLIENT *client, int id){
    if(client == NULL) return -1;

    pthread_mutex_lock(&client->lock);
    INVITATION *inv = client->invitation_list[id];
    if(inv==NULL){ // invitation does not exist;
        pthread_mutex_unlock(&client->lock);
        return -1;
    }
    if(inv_get_source(inv) == client){ // client is source
        GAME_ROLE source_role = inv_get_source_role(inv);
        if(inv_close(inv,source_role)){ // closing resulted in error
            pthread_mutex_unlock(&client->lock);
            return -1;
        }
        client_remove_invitation(client,inv);
        CLIENT *opp = inv_get_target(inv);
        client_ref(opp,"get the oppponent's reference\n");
        JEUX_PACKET_HEADER hdr = {0};
        hdr.type = JEUX_RESIGNED_PKT;
        int opp_id = -1;
        for(int i=0;i<255;i++){ // finding the opponent id
            if(opp->invitation_list[i] == inv) {opp_id = i; break;}
        }
        hdr.id = opp_id;
        client_send_packet(opp,&hdr,NULL);
        client_unref(opp,"reference released because packet sent to opponent\n");
    }

    if(inv_get_target(inv) == client){ // client is target
        GAME_ROLE target_role = inv_get_target_role(inv);
        if(inv_close(inv,target_role)){ // closing resulted in error
            pthread_mutex_unlock(&client->lock);
            return -1;
        }
        client_remove_invitation(client,inv);
        CLIENT *opp = inv_get_source(inv);
        client_ref(opp,"get the oppponent's reference\n");
        JEUX_PACKET_HEADER hdr = {0};
        hdr.type = JEUX_RESIGNED_PKT;
        int opp_id = -1;
        for(int i=0;i<255;i++){ // finding the opponent id
            if(opp->invitation_list[i] == inv) {opp_id = i; break;}
        }
        hdr.id = opp_id;
        client_send_packet(opp,&hdr,NULL);
        client_unref(opp,"reference released because packet sent to opponent\n");
    }
    pthread_mutex_unlock(&client->lock);
    return 0;
}

/*
 * Make a move in a game currently in progress, in which the specified
 * CLIENT is a participant.  The GAME in which the move is to be made is
 * specified by passing the ID assigned by the CLIENT to the INVITATION
 * that contains the game.  The move to be made is specified as a string
 * that describes the move in a game-dependent format.  It is an error
 * if the ID does not refer to an INVITATION containing a GAME in progress,
 * if the move cannot be parsed, or if the move is not legal in the current
 * GAME state.  If the move is successfully made, then a MOVED packet is
 * sent to the opponent of the CLIENT making the move.  In addition, if
 * the move that has been made results in the game being over, then an
 * ENDED packet containing the appropriate game ID and the game result
 * is sent to each of the players participating in the game, and the
 * INVITATION containing the now-terminated game is removed from the lists
 * of both the source and target.  The result of the game is posted in
 * order to update both players' ratings.
 *
 * @param client  The CLIENT that is making the move.
 * @param id  The ID assigned by the CLIENT to the GAME in which the move
 * is to be made.
 * @param move  A string that describes the move to be made.
 * @return 0 if the move was made successfully, -1 otherwise.
 */
int client_make_move(CLIENT *client, int id, char *move){
    return 0;
}
