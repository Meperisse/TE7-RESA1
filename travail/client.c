#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"

void echo_client(int sockfd) {

	char buff[MSG_LEN];
    struct pollfd fds[2];

    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = sockfd;
    fds[1].events = POLLIN;

	int n;
    int len;

	while (1) {

        int ret = poll(fds, 2, -1); // -1 = attente infinie // 2 descripteur de fichier
        if (ret < 0) {
            perror("poll");
            break;
        }

        if (fds[0].revents & POLLIN){

            memset(buff, 0, MSG_LEN); // clean le buffer en le mettant à 0

		    // Getting message from client
		    printf("Message: ");
		    n = 0;
		    while ((buff[n++] = getchar()) != '\n') {} // trailing '\n' will be sent
            buff[n-1] = '\0'
            len = n;

            if (strcmp(buff, "/quit") == 0) {
                printf("Fermeture de la connexion.\n");
                break;
            }

            // Sending size
            if (send(sockfd, &len, sizeof(int), 0) <= 0) {
                break;
            }

		    // Sending message
		    if (send(sockfd, buff, strlen(buff), 0) <= 0) {
			    break;
		    }
		    printf("Message sent!\n");
            }
		
            
        if (fds[1].revents & POLLIN){
            // Receiving size
            memset(buff, 0, MSG_LEN);  // on re-réinitialise
            if (recv(sockfd, &len, sizeof(int), 0) <= 0) {
                break;
            }

            // Receiving message
            if (recv(sockfd, buff, len, 0) <= 0) {
                break;
            }
            printf("Received: %s\n", buff);
            }
        
	}
}

int handle_connect(char* host, char* port) {
	struct addrinfo hints, *result, *rp;
	int sfd;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(host, port, &hints, &result) != 0) {
		perror("getaddrinfo()");
		exit(EXIT_FAILURE);
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,rp->ai_protocol);
		if (sfd == -1) {
			continue;
		}
		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
			break;
		}
		close(sfd);
	}
	if (rp == NULL) {
		fprintf(stderr, "Could not connect\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);
	return sfd;
}



int main(int argc, char* argv[]) {

    if (argc != 3){
        exit(EXIT_FAILURE);
    }

    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    
	int sfd;
	sfd = handle_connect(argv[1], argv[2]);
	echo_client(sfd);
	close(sfd);
	return EXIT_SUCCESS;
}

