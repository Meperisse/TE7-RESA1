#include "client_list.h"
#include "common.h"
#include <stdlib.h>

struct client_info {
	int fd;
	struct sockaddr_in address;
	char nick[NICK_LEN];
	time_t connect_time; 
	struct client_info *next;
};

int client_list_add(struct client_info **clients, int fd, const struct sockaddr_in *address) {
	struct client_info *client = malloc(sizeof(*client));
	if (client == NULL) {
		return -1;
	}
	client->fd = fd;
	client->address = *address;
	client->nick[0] = '\0';
	client->connect_time = -1;
	client->next = *clients;
	*clients = client;
	return 0;
}

void client_list_remove(struct client_info **clients, int fd) {
	struct client_info **cursor = clients;

	while (*cursor != NULL) {
		if ((*cursor)->fd == fd) {
			struct client_info *removed = *cursor;
			*cursor = removed->next;
			free(removed);
			return;
		}
		cursor = &(*cursor)->next;
	}
}

void client_list_destroy(struct client_info **clients) {
	while (*clients != NULL) {
		struct client_info *removed = *clients;
		*clients = removed->next;
		free(removed);
	}
}

struct client_info *client_list_find_by_fd(struct client_info *clients, int fd) {
	for (struct client_info *c = clients; c != NULL; c = c->next) {
		if (c->fd == fd) {
			return c;
		}
	}
	return NULL;
}

struct client_info *client_list_find_by_nick(struct client_info *clients, const char *nick) {
	if (nick == NULL || nick[0] == '\0') {
		return NULL; // un client sans pseudo ne doit jamais matcher
	}
	for (struct client_info *c = clients; c != NULL; c = c->next) {
		if (strcmp(c->nick, nick) == 0) {
			return c;
		}
	}
	return NULL;
}

int client_get_fd(const struct client_info *client) {
	return client->fd;
}

const char *client_get_nick(const struct client_info *client) {
	return client->nick;
}

const struct sockaddr_in *client_get_address(const struct client_info *client) {
	return &client->address;
}

time_t client_get_connect_time(const struct client_info *client) {
	return client->connect_time;
}

int client_set_nick(struct client_info *client, const char *nick) {
	if (strlen(nick) > NICK_LEN || strlen(nick)<1) {
		return -1;
	}
	strcpy(client->nick, nick);
	return 0;
}