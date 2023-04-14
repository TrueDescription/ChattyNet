/* server process */

/* include the necessary header files */
#include<ctype.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<stdlib.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<unistd.h>
#include<string.h>

#include<sys/select.h>

#include "protocol.h"
#include "libParseMessage.h"
#include "libMessageQueue.h"

int max(int a, int b) {
    if (a>b)return a;
    return b;
}

/**
 * @brief replaces a null terminator within the given string to a new line charachter 
 * 
 * @param nullTermString a string that is null terminated
 * @return int length i if succesfull, 0 on fail
 */
int toNewLine(char *nullTermString) {
	int i;
	for (i = 0; nullTermString[i] != '\0'; i++) continue;
	if(nullTermString[i]=='\0') {
		nullTermString[i]='\n';
		return i;
	}
	return 0;
}

/**
 * @return int length i if succesfull, 0 on fail
*/
int toNullTerm(char *newLineString, int numRecv) {
	int i;
	for (i = 0; i < numRecv && newLineString[i] != '\n'; i++) continue;
	if (newLineString[i]=='\n') {
		newLineString[i]='\0';
		return i;
	}
	return 0;
}

/**
 * send a single message to client 
 * sockfd: the socket to read from
 * toClient: a buffer containing a null terminated string with length at most 
 * 	     MAX_MESSAGE_LEN-1 characters. We send the message with \n replacing \0
 * 	     for a mximmum message sent of length MAX_MESSAGE_LEN (including \n).
 * return 1, if we have successfully sent the message
 * return 2, if we could not write the message
 */
int sendMessage(int sfd, char *toClient){
	int i;
	if ( (i = toNewLine(toClient)) == 0) return 2; 
	int numSend = send(sfd, toClient, i + 1, 0);
	if(numSend!=1)return(2);
	return(1);
}

/**
 * read a single message from the client. 
 * sockfd: the socket to read from
 * fromClient: a buffer of MAX_MESSAGE_LEN characters to place the resulting message
 *             the message is converted from newline to null terminated, 
 *             that is the trailing \n is replaced with \0
 * return 1, if we have received a newline terminated string
 * return 2, if the socket closed (read returned 0 characters)
 * return 3, if we have read more bytes than allowed for a message by the protocol
 */
int recvMessage(int sfd, char *incomingBuffer){
	char buffer[MAX_MESSAGE_LEN];
	int numRecv = recv(sfd, buffer, MAX_MESSAGE_LEN, 0);
	if(numRecv==0)return(2);
	int i;
	for (i = 0; i < numRecv; i++) {
		if (buffer[i] == '\n') {
			i++;
			break;
		}
		continue;
	}
	if(i == MAX_MESSAGE_LEN && buffer[numRecv - 1]!='\n')return(3);
	if(strlen(incomingBuffer) + numRecv >= MAX_MESSAGE_LEN)return(3);
	strncat(incomingBuffer, buffer, numRecv);
	return(1);
}

/**
 * Extract a new line terminated string from incomingBuffer to fromClient
 * return 1 if there is a partial message left in the buffer, 0 otherwise
*/
int extractMessage(char *incomingBuffer, char *fromClient) {
	int i;
    for (i = 0; incomingBuffer[i] != '\n'; i++) {
        if ( incomingBuffer[i] == '\0') return(0);
    }
	incomingBuffer[i] = '\0';
    strncpy(fromClient,  incomingBuffer, i+1);
    strcpy(incomingBuffer, incomingBuffer + i + 1);
    return(i);
}


int main (int argc, char ** argv) {
    int sockfd;

    if(argc!=2){
	    fprintf(stderr, "Usage: %s portNumber\n", argv[0]);
	    exit(1);
    }
    int port = atoi(argv[1]);

    if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) == -1) {
        perror ("socket call failed");
        exit (1);
    }

    struct sockaddr_in server;
    server.sin_family=AF_INET;          // IPv4 address
    server.sin_addr.s_addr=INADDR_ANY;  // Allow use of any interface 
    server.sin_port = htons(port);      // specify port

    if (bind (sockfd, (struct sockaddr *) &server, sizeof(server)) == -1) {
        perror ("bind call failed");
        exit (1);
    }

    if (listen (sockfd, 5) == -1) {
        perror ("listen call failed");
        exit (1);
    }
	MessageQueue UserArray[32];
	MessageQueue OutgoingArray[32];
	char incommingBuffer[32][MAX_CHAT_MESSAGE_LEN];
	for (int q = 0; q < 32; q++) {
		initQueue(&UserArray[q]);
		initQueue(&OutgoingArray[q]);
		incommingBuffer[q][0] = '\0';
	}
	//char UserNameArray[32][MAX_USER_LEN];
	//int fdlist[33];
    int fdcount=0;
    //fdlist[0]=sockfd;
	fdcount++;
    for (;;) {
		fd_set readfds, writefds, exceptfds;
		FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_ZERO(&exceptfds);
		int fdmax=0;  
		FD_SET(sockfd, &readfds);
		fdmax=max(fdmax,sockfd);                        
        for (int i=0; i<32; i++) {
            if (UserArray[i].active) {
                FD_SET(UserArray[i].fd, &readfds);
				FD_SET(UserArray[i].fd, &writefds);
                fdmax=max(fdmax,UserArray[i].fd);
            }
        }
		struct timeval tv;
        tv.tv_sec=5;          
        tv.tv_usec=0;

		int numfds;
		
		if ((numfds=select(fdmax+1, &readfds, &writefds, NULL, &tv))>0) {
				// change loop to go through all 32 items or until fcount iterations
				// change the user arrays to signify activation or not
				// when you quit, de active and zero every buffer, dequeu and write everything
				// only enter if active and FD_ISSSET
			 for (int i=-1; i<32; i++) {
				int qFlag = 0;
				int kFlag = -1;
				//int fd;
				if (i >= 0 && ! UserArray[i].active)continue;
				if (i == -1 && FD_ISSET(sockfd, &readfds))  /*accept a connection */{
					int newsockfd;
					if ((newsockfd = accept (sockfd, NULL, NULL)) == -1) {
						perror ("accept call failed");
						continue;
					}
					// loop until you find the first inactive array
					// set fd to newsockfd
					if (fdcount == 32) {
						close(newsockfd);
						continue;
					}
					int u;
					for (u = 0; u < 32; u++) {
						if (! UserArray[u].active) break;
					}
					UserArray[u].userName[0] = '\0';
					UserArray[u].fd = newsockfd;
					UserArray[u].active = 1;
					fdcount++;
					continue;
				}
				if (UserArray[i].active && FD_ISSET(UserArray[i].fd,&readfds)) {
					char fromClient[MAX_CHAT_MESSAGE_LEN], toClient[MAX_MESSAGE_LEN];				
					int retVal=recvMessage(UserArray[i].fd, incommingBuffer[i]); 
					int res;
					if (retVal == 3) {
						qFlag = 2;
						kFlag = i;
					}
					else if (retVal == 2) {
						continue;
					}
					while (kFlag != i && (res = extractMessage(incommingBuffer[i], fromClient) > 0)) {
						char *part[4];
						int numParts=parseMessage(fromClient, part);
						if(numParts==0){
							strcpy(toClient,"ERROR");
						} 
						else if(strcmp(part[0], "list")==0) {
							sprintf(toClient, "users:");
							int total = 0;
							for (int uc = 0; uc < 32 && total < 10; uc++) {
								if (! UserArray[uc].active) continue;
								if (UserArray[uc].userName[0] != '\0') {
									strncat(toClient, UserArray[uc].userName, MAX_USER_LEN);
									strcat(toClient, ":");
									total++;
								}
							}
						} 
						else if(strcmp(part[0], "message")==0) {
							char *fromUser=part[1];
							char *toUser=part[2];
							char *message=part[3];
							int u;
							for (u = 0; u<32; u++) {
								if (strcmp(toUser, UserArray[u].userName) == 0 && UserArray[u].active) {
									break;
								}
							}
							if(strcmp(fromUser, UserArray[i].userName)!=0) {
								sprintf(toClient, "invalidFromUser:%s",fromUser);
							} 
							else if(u == 32) { 
								sprintf(toClient, "invalidToUser:%s ",toUser);
							} 
							else {
								sprintf(toClient, "%s:%s:%s:%s","message", fromUser, toUser, message);
								if(enqueue(&UserArray[u], toClient)) {
									strcpy(toClient, "messageQueued");
								}
								else {
									strcpy(toClient, "messageNotQueued");
									qFlag = 2;
									kFlag = u;
								}
							}
						}
						else if(strcmp(part[0], "quit")==0) {
							strcpy(toClient, "closingConnection");
							qFlag = 1;
							kFlag = i;
						} 
						else if(strcmp(part[0], "getMessage")==0){
							if(dequeue(&UserArray[i], toClient) == 0){
								strcpy(toClient, "noMessage");
							}
						} 
						else if(strcmp(part[0], "register")==0){
							
							if (strlen(UserArray[i].userName) <= 0 && strlen(part[1]) > 0){
								int cu;
								for (cu = 0; cu<32; cu++) {
									if (strcmp(part[1], UserArray[cu].userName) == 0) {
										break;
									}
								}
								if (cu != 32) {
									strcpy(toClient, "userAlreadyRegistered");
								}
								else if(strncmp(UserArray[i].userName, part[1], MAX_USER_LEN)!=0) {
									strcpy(UserArray[i].userName, part[1]);
									strcpy(toClient, "registered");
								} 
							}
							else {
								strcpy(toClient, "ERROR");
							}
						}
						if (enqueue(&OutgoingArray[i], toClient) == 0) {
							qFlag = 2;
							kFlag = i;
						}
						if (qFlag) /* clean up and quit */{
							if (FD_ISSET(UserArray[kFlag].fd, &writefds) && qFlag == 1) {
								sendMessage(UserArray[kFlag].fd, toClient);
							}
							while (dequeue(&OutgoingArray[kFlag], toClient) == 1) {
								continue;
							}
							while (dequeue(&UserArray[kFlag], toClient) == 1) {
								continue;
							}
							close(UserArray[kFlag].fd);
							UserArray[kFlag].active = 0;
							UserArray[kFlag].fd = -1;
							UserArray[kFlag].userName[0] = '\0';
							kFlag = -1;
							qFlag = 0;
							fdcount--;
							continue;
						}
					}
					
				}
				if (FD_ISSET(UserArray[i].fd,&writefds) && UserArray[i].active) {
					/* code */
					// dequeue a message from the outgoing buffer and send it
					char toClient[MAX_MESSAGE_LEN];
					if (dequeue(&OutgoingArray[i], toClient) == 0) continue;
					sendMessage(UserArray[i].fd, toClient);
				}
				
			}	
		}
    }
    exit(0);
}
