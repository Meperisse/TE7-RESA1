#include "common.h"
#include "client_list.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>

#define MAX_MESSAGE_SIZE 4096
#define MAX_CLIENTS 128

int setup_listening_socket(int port) {
	int listen_fd;
	int result;
	struct sockaddr_in server_address;

	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	die(listen_fd, "socket");
	printf("TCP listening socket created.\n");

	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY); // To listen on all interfaces --- Equivalent to 0.0.0.0
	server_address.sin_port = htons((unsigned short)port);
	result = bind(listen_fd, (struct sockaddr *)&server_address, sizeof(server_address));
	die(result, "bind");
	printf("Socket bound to port %d.\n", port);

	result = listen(listen_fd, 20);
	die(result, "listen");
	printf("Listening for client connections.\n");
	return listen_fd;
}

void accept_and_insert_client(int listen_fd, struct pollfd poll_fds[MAX_CLIENTS], struct client_info **clients) {
	struct sockaddr_in client_address;
	socklen_t client_address_length = sizeof(client_address);
	int client_fd = accept(listen_fd, (struct sockaddr *)&client_address,
		&client_address_length);
	int slot;

	die(client_fd, "accept");
	for (slot = 1; slot < MAX_CLIENTS; slot++) {
		if (poll_fds[slot].fd < 0) {
			if (client_list_add(clients, client_fd, &client_address) < 0) {
				close(client_fd);
				die(-1, "malloc client information");
			}
			poll_fds[slot].fd = client_fd;
			poll_fds[slot].events = POLLIN;
			poll_fds[slot].revents = 0;
			printf("Accepted client %s:%u on slot %d.\n",
				inet_ntoa(client_address.sin_addr),
				(unsigned int)ntohs(client_address.sin_port), slot);
			break;
		}
	}
	if (slot == MAX_CLIENTS) {
		fprintf(stderr, "Client limit reached. Closing the new connection.\n");
		close(client_fd);
	}
}

/* Envoie une réponse du serveur. Retourne 1 si OK, -1 si erreur d'écriture. */
int send_reply(int fd, enum msg_type type, const char *infos, const char *text) {
	struct message msg;
	size_t len = strlen(text);

	if (len > MAX_MESSAGE_SIZE) {
		len = MAX_MESSAGE_SIZE;
	}
	memset(&msg, 0, sizeof(msg));          // évite d'envoyer des octets de padding non initialisés
	strcpy(msg.nick_sender, "Server");
	snprintf(msg.infos, sizeof(msg.infos), "%s", infos);
	msg.type = type;
	msg.pld_len = (int)len;
	return send_message(fd, &msg, text);
}

/* Non vide, lettres et chiffres uniquement */
int is_valid_nick(const char *nick) {
	if (nick[0] == '\0') {
		return 0;
	}
	for (size_t i = 0; nick[i] != '\0'; i++) {
		if (!isalnum((unsigned char)nick[i])) {
			return 0;
		}
	}
	return 1;
}

//* Retourne 1 si le client doit être déconnecté, 0 sinon. */
int handle_client_message(int client_fd, struct client_info *clients) {
	struct message msg;
	char payload[MAX_MESSAGE_SIZE + 1];
	struct client_info *sender;
	int r = recv_message(client_fd, &msg, payload, sizeof(payload));

	if (r <= 0) {
		fprintf(stderr, "Client %d : %s\n", client_fd,
			r == 0 ? "Socket close" : "Read error or invalid message");
		return 1;
	}
	sender = client_list_find_by_fd(clients, client_fd);
	if (sender == NULL) {
		return 1;                           // ne devrait jamais arriver
	}
	/* On n'a pas confiance en nick_sender reçu : on le remplace par le pseudo stocké */
	snprintf(msg.nick_sender, sizeof(msg.nick_sender), "%s", client_get_nick(sender));
	printf("Client %d : %s\n", client_fd, msg_type_str[msg.type]);

	if (msg.type == ECHO_SEND && strcmp(payload, "/quit") == 0) {
		printf("Client %d requested to quit.\n", client_fd);
		return 1;
	}
	if (client_get_nick(sender)[0] == '\0' && msg.type != NICKNAME_NEW) {
		return send_reply(client_fd, msg.type, "",
			"Please choose a nickname with /nick <nickname>") < 0;
	}

	switch (msg.type) {
	case NICKNAME_NEW:
		return handle_nickname_new(client_fd, sender, clients, &msg);
	case NICKNAME_LIST:
		return handle_nickname_list(client_fd, clients);
	case NICKNAME_INFOS:
		return handle_nickname_infos(client_fd, clients, &msg);
	case ECHO_SEND:
		return send_message(client_fd, &msg, payload) < 0;   // Req2.11
	case UNICAST_SEND:
		return handle_unicast(client_fd, clients, &msg, payload);
	case BROADCAST_SEND:
		return handle_broadcast(sender, clients, &msg, payload);
	default:
		return send_reply(client_fd, msg.type, "", "Unsupported command") < 0;
	}
}

void server_poll_loop(int listen_fd, struct pollfd poll_fds[MAX_CLIENTS],
		struct client_info **clients) {
	int running = 1;

	/* Slot 0 is the listener. The other slots contain client sockets. */
	for (int i = 0; i < MAX_CLIENTS; i++) {
		poll_fds[i].fd = -1;
		poll_fds[i].events = 0;
		poll_fds[i].revents = 0;
	}
	poll_fds[0].fd = listen_fd;
	poll_fds[0].events = POLLIN;

	// execute server logic
	while (running) {
		int ready = poll(poll_fds, MAX_CLIENTS, -1);
		die(ready, "poll");

		if ((poll_fds[0].revents & POLLIN) != 0) {
			accept_and_insert_client(listen_fd, poll_fds, clients);
		}

		for (int slot = 1; slot < MAX_CLIENTS; slot++) {
			short returned_events = poll_fds[slot].revents;
			int close_connection = 0;
			if (poll_fds[slot].fd < 0) {
				continue;
			}

			if ((returned_events & POLLIN) != 0) {
				close_connection = handle_client_message(poll_fds[slot].fd, *clients);
			}
			if ((returned_events & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
				close_connection = 1;
			}
			if (close_connection) {
				int client_fd = poll_fds[slot].fd;
				close(client_fd);
				client_list_remove(clients, client_fd);
				poll_fds[slot].fd = -1;
				poll_fds[slot].events = 0;
				poll_fds[slot].revents = 0;
			}
		}
		if ((poll_fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
			running = 0;
		}
	}

	// Cleaning up: close all client sockets and free the client list
	for (int slot = 1; slot < MAX_CLIENTS; slot++) {
		if (poll_fds[slot].fd >= 0) {
			close(poll_fds[slot].fd);
			poll_fds[slot].fd = -1;
		}
	}
	client_list_destroy(clients);
}

int main(int argc, char **argv) {
	struct pollfd poll_fds[MAX_CLIENTS];
	struct client_info *clients = NULL;
	int port;
	int listen_fd;

	if (argc != 2) {
		fprintf(stderr, "Usage: ./server <server_port>\n");
		return EXIT_FAILURE;
	}
	port = atoi(argv[1]);
	if (port < 1 || port > 65535) {
		fprintf(stderr, "Invalid port\n");
		return EXIT_FAILURE;
	}

	listen_fd = setup_listening_socket(port);
	server_poll_loop(listen_fd, poll_fds, &clients);
	close(listen_fd);
	return EXIT_SUCCESS;
}
