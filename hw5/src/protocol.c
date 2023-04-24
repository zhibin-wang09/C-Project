#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "protocol.h"

ssize_t rio_writen(int fd, void *usrbuf, size_t n);
ssize_t rio_readn(int fd, void *usrbuf, size_t n);

int proto_send_packet(int fd, JEUX_PACKET_HEADER *hdr, void *data){
	int hdr_size = ntohs(hdr -> size);
	if(rio_writen(fd,hdr,sizeof(JEUX_PACKET_HEADER)) < 0) { close(fd); perror("writing header to wire failed\n"); return -1;} // write header
	if(hdr_size > 0 && data != NULL){ // contains data then additional write
		if(rio_writen(fd,data,hdr_size) < 0) { close(fd); perror("writing payload to wire failed\n"); return -1;}
	}
	return 0;
}

int proto_recv_packet(int fd, JEUX_PACKET_HEADER *hdr, void **payloadp){
	int ret;
	if((ret = rio_readn(fd,hdr,sizeof(JEUX_PACKET_HEADER))) == 0) { close(fd); return -1;} // read header, close fd if eof
	if(ret < 0){close(fd);perror("reading header failed\n");return -1;} // if error close fd as well and print error message
	int size = ntohs(hdr->size);
	if(size > 0){ // received packet contains paylaod
		*payloadp= malloc(size);
		if(*payloadp == NULL) { close(fd); perror("malloc for payload failed\n"); return -1;}
		if(rio_readn(fd,*payloadp,size) < 0) {
			close(fd);
			if(*payloadp != NULL) free(*payloadp);
			perror("reading payload faild\n"); return -1;
		}
	}

	return 0;
}

ssize_t rio_readn(int fd, void *usrbuf, size_t n) {
    size_t nleft = n;
    ssize_t nread;
    char *bufp = usrbuf;

    while (nleft > 0) {
	if ((nread = read(fd, bufp, nleft)) < 0) {
	    if (errno == EINTR) /* Interrupted by sig handler return */
		nread = 0;      /* and call read() again */
	    else
		return -1;      /* errno set by read() */
	}
	else if (nread == 0)
	    break;              /* EOF */
	nleft -= nread;
	bufp += nread;
    }
    return (n - nleft);         /* Return >= 0 */
}

ssize_t rio_writen(int fd, void *usrbuf, size_t n){
    size_t nleft = n;
    ssize_t nwritten;
    char *bufp = usrbuf;

    while (nleft > 0) {
	if ((nwritten = write(fd, bufp, nleft)) <= 0) {
	    if (errno == EINTR)  /* Interrupted by sig handler return */
		nwritten = 0;    /* and call write() again */
	    else
		return -1;       /* errno set by write() */
	}
	nleft -= nwritten;
	bufp += nwritten;
    }
    return n;
}