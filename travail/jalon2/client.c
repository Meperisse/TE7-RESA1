#include "common.h"
#include "msg_struct.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_MESSAGE_SIZE 4096

/* Pseudo actuel du client ("" tant qu'aucun pseudo n'est attribué). */
static char my_nick[NICK_LEN] = "";

int setup_connection(const char *server_ip, const char *server_port) {
    int socket_fd;
    int result;
    struct sockaddr_in server_address;

    printf("Using server IPv4 address %s.\n", server_ip);
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    result = inet_aton(server_ip, &server_address.sin_addr);
    if (result == 0) {
        fprintf(stderr, "Invalid IPv4 address: %s\n", server_ip);
        return -1;
    }

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    die(socket_fd, "socket");
    printf("TCP socket created.\n");

    server_address.sin_port = htons((unsigned short)atoi(server_port));
    result = connect(socket_fd, (struct sockaddr *)&server_address, sizeof(server_address));
    die(result, "connect");
    printf("Connected to %s:%s.\n", inet_ntoa(server_address.sin_addr), server_port);
    return socket_fd;
}

/* Req2.0 : envoie une struct message puis le payload si pld_len > 0. */
static int send_message(int socket_fd, enum msg_type type, const char *infos,
                        const char *payload, int pld_len) {
    struct message msg;

    memset(&msg, 0, sizeof(msg));
    msg.pld_len = pld_len;
    msg.type = type;
    strncpy(msg.nick_sender, my_nick, NICK_LEN - 1);
    strncpy(msg.infos, infos, INFOS_LEN - 1);

    if (write_in_socket(socket_fd, &msg, sizeof(msg)) == 0) {
        return 0;
    }
    if (pld_len > 0 && write_in_socket(socket_fd, (void *)payload, (size_t)pld_len) == 0) {
        return 0;
    }
    return 1;
}

/* Req2.1 : lettres/chiffres uniquement, non vide, longueur < NICK_LEN. */
static int is_valid_nick(const char *nick) {
    size_t len = strlen(nick);
    size_t i;

    if (len == 0 || len >= NICK_LEN) {
        return 0;
    }
    for (i = 0; i < len; i++) {
        if (!isalnum((unsigned char)nick[i])) {
            return 0;
        }
    }
    return 1;
}

/* Req2.0 : reçoit une struct message puis son payload.
 * Return 1 to keep running, or 0 if the server disconnects or sends an invalid message. */
int read_server_message(int socket_fd) {
    struct message msg;
    char payload[MAX_MESSAGE_SIZE + 1];

    if (read_from_socket(socket_fd, &msg, sizeof(msg)) == 0) {
        return 0;
    }
    if (msg.pld_len < 0 || msg.pld_len > MAX_MESSAGE_SIZE) {
        fprintf(stderr, "Invalid payload size from server: %d\n", msg.pld_len);
        return 0;
    }
    if (msg.pld_len > 0 && read_from_socket(socket_fd, payload, (size_t)msg.pld_len) == 0) {
        return 0;
    }
    payload[msg.pld_len] = '\0';

    /* Req2.4 : confirmation d'un /nick accepté (convention à valider avec server.c). */
    if (msg.type == NICKNAME_NEW) {
        strncpy(my_nick, msg.infos, NICK_LEN - 1);
        my_nick[NICK_LEN - 1] = '\0';
    }

    if (msg.pld_len > 0) {
        printf("%s\n", payload);
    }
    return 1;
}

/* Req2.1, 2.5, 2.6, 2.7 (et Req1.7 pour /quit).
 * Return 1 to keep running, or 0 when stdin closes, the user quits or sending fails. */
int get_and_send_user_message(int socket_fd) {
    char line[MAX_MESSAGE_SIZE + 1];
    ssize_t bytes_read;
    size_t len;

    bytes_read = read(STDIN_FILENO, line, MAX_MESSAGE_SIZE);
    die(bytes_read, "read stdin");
    if (bytes_read == 0) {
        return 0;
    }
    len = (size_t)bytes_read;
    line[len] = '\0';
    if (len > 0 && line[len - 1] == '\n') {
        line[--len] = '\0';
    }
    if (len == 0) {
        return 1;
    }

    /* Req1.7 : toujours obligatoire, pas de type dédié au jalon 2
     * (convention à valider avec server.c). */
    if (strcmp(line, "/quit") == 0) {
        send_message(socket_fd, ECHO_SEND, "", "/quit", 5);
        return 0;
    }

    /* Req2.1 / Req2.4 : nouveau pseudo ou changement de pseudo. */
    if (strncmp(line, "/nick ", 6) == 0) {
        const char *nick = line + 6;

        if (!is_valid_nick(nick)) {
            fprintf(stderr, "Invalid nickname (letters and digits only, max %d).\n", NICK_LEN - 1);
            return 1;
        }
        return send_message(socket_fd, NICKNAME_NEW, nick, NULL, 0);
    }

    /* Req2.5 : liste des utilisateurs connectés. */
    if (strcmp(line, "/who") == 0) {
        return send_message(socket_fd, NICKNAME_LIST, "", NULL, 0);
    }

    /* Req2.6 : informations sur un utilisateur. */
    if (strncmp(line, "/whois ", 7) == 0) {
        const char *nick = line + 7;

        if (!is_valid_nick(nick)) {
            fprintf(stderr, "Usage: /whois <nickname>\n");
            return 1;
        }
        return send_message(socket_fd, NICKNAME_INFOS, nick, NULL, 0);
    }

    /* Req2.7 : diffusion à tous les autres utilisateurs. */
    if (strncmp(line, "/msgall ", 8) == 0) {
        const char *text = line + 8;

        if (*text == '\0') {
            fprintf(stderr, "Usage: /msgall <message>\n");
            return 1;
        }
        return send_message(socket_fd, BROADCAST_SEND, "", text, (int)strlen(text));
    }

    fprintf(stderr, "Unknown or unsupported command: %s\n", line);
    return 1;
}

/* Req1.5 : un seul poll() sur stdin et la socket. */
void client_poll_loop(int socket_fd) {
    struct pollfd watched[2];
    int running = 1;

    watched[0].fd = STDIN_FILENO;
    watched[0].events = POLLIN;
    watched[1].fd = socket_fd;
    watched[1].events = POLLIN;

    while (running) {
        int ready = poll(watched, 2, -1);
        die(ready, "poll");

        if ((watched[1].revents & POLLIN) != 0) {
            running = read_server_message(socket_fd);
        }

        if (running && (watched[0].revents & POLLIN) != 0) {
            running = get_and_send_user_message(socket_fd);
        }

        if ((watched[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0 ||
            (watched[1].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            running = 0;
        }
    }
}

int main(int argc, char **argv) {
    int socket_fd;

    if (argc != 3) {
        fprintf(stderr, "Usage: ./client <server_ipv4> <server_port>\n");
        return EXIT_FAILURE;
    }
    socket_fd = setup_connection(argv[1], argv[2]);
    if (socket_fd < 0) {
        return EXIT_FAILURE;
    }
    client_poll_loop(socket_fd);
    close(socket_fd);
    return EXIT_SUCCESS;
}