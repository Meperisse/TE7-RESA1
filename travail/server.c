#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>

#define FDS_SIZE 128
#define BACK_LOG 20

void die(int ret_value, const char* msg){
    if (ret_value<0){
        perror(msg);
        exit(EXIT_FAILURE);
    }
}

void socketcreation(int *fd, struct sockaddr_in *engine_addr, int port, char* addr){
    *fd  = socket(AF_INET, SOCK_STREAM, 0);
    die(*fd, "Socket creation");
    engine_addr->sin_family = AF_INET;
    engine_addr->sin_port = htons(port);
    engine_addr->sin_addr.s_addr = inet_addr(addr);
    int yes=1;
    setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
}

void socketactivation(int *fd, struct sockaddr_in *engine_addr){
    int ret;
    ret = bind(*fd, (struct sockaddr*)engine_addr, sizeof(*engine_addr));
    die(ret, "Binding");
    ret = listen(*fd, BACK_LOG);
    die(ret, "On listening");
}

void fdsalteration(struct pollfd *fds, int idx, int fd, int bitmask, int revent){
    fds[idx].fd=fd;
    fds[idx].events=bitmask;
    fds[idx].revents=revent;
}

int findidx(struct pollfd *fds, int size){
    for (int idx=0; idx < FDS_SIZE; idx++){
        if (fds[idx].revents !=0){
            return idx;
        }
    }
    return -1;
}

int main(int argc, char* argv[]) {
    // Récupération du numéro de port
    int PORT = atoi(argv[1]); // sécurité à rajouter
    // Création de la socket d'écoute
    int welcome_fd;
    struct sockaddr_in server_addr;
    socketcreation(&welcome_fd, &server_addr, PORT, "127.0.0.1");
    socketactivation(&welcome_fd, &server_addr);
    // Création de l'array pour poll
    struct pollfd fds[FDS_SIZE]={0}; 
    fdsalteration(&fds, 0, welcome_fd, POLLIN, 0);
    int next_place = 1;
    while (1) {
        poll(fds, FDS_SIZE,-1); //bloquant
        for (int idx=0, idx < FDS_SIZE, idx++){
            idx = findidx(&fds);
            if (idx==0){ // Si reception sur la socket d'écoute
                if (next_place == FDS_SIZE){
                    printf("Maximum connection");
                    exit(EXIT_FAILURE);
                }
                int new_fd = accept(welcome_fd, NULL, NULL);
                fdsalteration(&fds, next_place, new_fd, POLLIN, 0);
                fds[0].revents = 0;
                next_place +=1;
            } else { // Si reception sur une autre socket que celle d'écoute

            }
        }
    }

    
}