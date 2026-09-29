#ifndef JALON2_CLIENT_LIST_H
#define JALON2_CLIENT_LIST_H

#include <netinet/in.h>
#include <time.h> 

struct client_info;

int client_list_add(struct client_info **clients, int fd, const struct sockaddr_in *address);
void client_list_remove(struct client_info **clients, int fd);
void client_list_destroy(struct client_info **clients);
struct client_info *client_list_find_by_fd(struct client_info *clients, int fd);
struct client_info *client_list_find_by_nick(struct client_info *clients, const char *nick);
int client_get_fd(const struct client_info *client);
const char *client_get_nick(const struct client_info *client);
const struct sockaddr_in *client_get_address(const struct client_info *client);
time_t client_get_connect_time(const struct client_info *client);
int client_set_nick(struct client_info *client, const char *nick);
#endif
