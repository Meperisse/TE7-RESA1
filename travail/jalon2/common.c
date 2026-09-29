#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void die(int val, char *msg) {
	if (val < 0) {
		perror(msg);
		exit(EXIT_FAILURE);
	}
}

int read_from_socket(int fd, void *buf, size_t msg_size) {
	size_t total = 0;
	char *cursor = buf;

	while (total < msg_size) {
		ssize_t n = read(fd, cursor + total, msg_size - total);
		die((int)n, "read");
		if (n == 0) {
			return 0;
		}
		total += (size_t)n;
	}
	return (int)total;
}

int write_in_socket(int fd, const void *buf, size_t msg_size) {
	size_t total = 0;
	const char *cursor = buf;

	while (total < msg_size) {
		ssize_t n = write(fd, cursor + total, msg_size - total);
		die((int)n, "write");
		if (n == 0) {
			return 0;
		}
		total += (size_t)n;
	}
	return (int)total;
}

int recv_message(int fd, struct message *msg, char *payload, size_t payload_cap) {
	int r = read_from_socket(fd, msg, sizeof(*msg));
	if (r <= 0) { 
		return r;
	}
	
	if (msg->type < 0 || msg->type > FILE_ACK) {
		return -1;
	}
	if (msg->pld_len < 0 || (size_t)msg->pld_len >= payload_cap) {
		return -1;
	}
	if (msg->pld_len > 0) {
		r = read_from_socket(fd, payload, (size_t)msg->pld_len);
		if (r <= 0) {
			return r;
		}
	}
	//payload[msg->pld_len] = '\0';   
	return 1;
}