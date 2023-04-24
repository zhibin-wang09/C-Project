#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <debug.h>

#include "jeux_globals.h"
#include "server.h"
#include "client_registry.h"
#include "player_registry.h"
#include "client.h"
#include "player.h"
#include "protocol.h"
#include "invitation.h"
#include "game.h"

void *jeux_client_service(void *arg){
	int connfd = *(int *)arg;
	free(arg); // free the arg because it is a heap space variable
	pthread_detach(pthread_self());
	CLIENT *client = creg_register(client_registry,connfd);
	CLIENT *target;
	int loggedin = 0;
	PLAYER *player = NULL;
	while(1){
		JEUX_PACKET_HEADER header= {0}; // the packet header holder
		void *payload = NULL; // the payload
		if(proto_recv_packet(connfd,&header,&payload) == -1){ // unblock until packet arrive and update header and payload if any
			debug("Ending client service...\n");
			creg_unregister(client_registry,client);
			break;
		}
		switch(header.type){
			case JEUX_LOGIN_PKT: // only login packet are allowed for the first iteration
				if(!payload) continue;
				if(loggedin){ // client is already logged in or there is some other user logged in
					free(payload);
					client_send_nack(client);
					debug("Already loggedin\n");
					continue;
				}
				debug("[5] LOGIN packet received\n");
				char *name = (char *) payload;
				player = preg_register(player_registry,name);
				if(client_login(client,player)){ // in case of error
					perror("client login error\n");
					player_unref(player,"login error\n");
					client_send_nack(client);
					free(payload);
					continue;
				}
				client_send_ack(client,NULL,0);
				loggedin = 1;
				break;
			case JEUX_USERS_PKT:
				if(!loggedin){ // client is already logged in or there is some other user logged in
					client_send_nack(client);
					continue;
				}
				PLAYER ** users = creg_all_players(client_registry); // obtain all players
				int num_users =0;
				PLAYER **players = users;
				while(*players != NULL){ // count number of players
					players++;
					num_users++;
				}
				// allocate space for the text string
				char *packet_payload = malloc(1);
				for(int i =0; i< num_users;i++){
					char user_info[528];
					//construct a text string for one user
					snprintf(user_info, 528, "%s\t%d\n",player_get_name(users[i]),player_get_rating(users[i]));
					//concatnate all user text strings
					size_t packet_payload_len = strlen(packet_payload);
				    size_t user_info_len = strlen(user_info);
				    packet_payload = realloc(packet_payload, packet_payload_len + user_info_len + 1); // resize the packet_payload buffer
				    strncat(packet_payload, user_info, user_info_len); // concatenate user_info to packet_payload
				    packet_payload[packet_payload_len + user_info_len] = '\0'; // add null terminator to packet_payload
				}
				//fprintf(stdout,"%s",packet_payload);
				for(int i=0;i<num_users;i++){
					player_unref(users[i],"packet sent already");
				}
				client_send_ack(client,packet_payload,strlen(packet_payload));
				break;
			case JEUX_INVITE_PKT:
				if(!loggedin){ // client is already logged in or there is some other user logged in
					client_send_nack(client);
					continue;
				}
				// ACK is sent to sender in success, and INVITED packet is sent to target
				char *user_name = (char *) payload;
				int target_role = header.role;
				if(target_role != 1 && target_role != 2){ // invalid role
					client_send_nack(client);
					free(payload);
					continue;
				}
				if(!(target = creg_lookup(client_registry,user_name)) || (target == client)){
					// no user exist or source and target are the same.
					client_send_nack(client);
					client_unref(target,"same target\n");
					free(payload);
					continue;
				}
				int source_role = target_role == 1 ? 2 : 1; // source role is the opposite of the target
				int id = client_make_invitation(client,target,source_role,target_role); // create invitation and obtain id
				if(id < 0){ // invalid invitation
					free(payload);
					client_send_nack(client);
					continue;
				}
				JEUX_PACKET_HEADER ack = {0};
				ack.type = JEUX_ACK_PKT;
				ack.id =id;
				client_unref(target,"done using target therefore discarded\n");
				client_send_packet(client,&ack,NULL); // send ack to the sender
				break;
			case JEUX_REVOKE_PKT:
				if(!loggedin){ // client is already logged in or there is some other user logged in
					client_send_nack(client);
					continue;
				}
				if(client_revoke_invitation(client,header.id)){
					client_send_nack(client);
					continue;
				}
				client_send_ack(client,NULL,0);
				//target should be sent a revoke packet on success
				break;
			case JEUX_DECLINE_PKT:
				if(!loggedin){ // client is already logged in or there is some other user logged in
					client_send_nack(client);
					continue;
				}
				if(client_decline_invitation(client,header.id)){
					client_send_nack(client);
					continue;
				}
				client_send_ack(client,NULL,0);
				break;
			case JEUX_ACCEPT_PKT:
				if(!loggedin){ // client is already logged in or there is some other user logged in
					client_send_nack(client);
					continue;
				}
				// this packet is sent by target
				char *initial_state;
				if(client_accept_invitation(client, header.id,&initial_state)){
					client_send_nack(client);
					continue;
				}
				if(initial_state){
					client_send_ack(client, initial_state, strlen(initial_state));
				}else{
					client_send_ack(client, NULL, 0);
				}

				break;
			case JEUX_MOVE_PKT:
				if(!loggedin){ // client is already logged in or there is some other user logged in
					client_send_nack(client);
					continue;
				}
				char *move = (char *) payload;
				if(client_make_move(client, header.id,move)){
					client_send_nack(client);
					free(payload);
					continue;
				}
				client_send_ack(client,NULL,0);
				break;
			case JEUX_RESIGN_PKT:
				if(!loggedin){ // client is already logged in or there is some other user logged in
					client_send_nack(client);
					continue;
				}
				if(client_resign_game(client,header.id)){
					client_send_nack(client);
					continue;
				}
				client_send_ack(client, NULL,0);
				break;
			default:
				break;
		}
	}
	return NULL;
}