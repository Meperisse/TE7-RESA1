#ifndef JALON2_COMMON_H
#define JALON2_COMMON_H

#define NICK_LEN 128
#define INFOS_LEN 128

enum msg_type {
    NICKNAME_NEW,
    NICKNAME_LIST,
    NICKNAME_INFOS,
    ECHO_SEND,
    UNICAST_SEND,
    BROADCAST_SEND,
    MULTICAST_CREATE,
    MULTICAST_LIST,
    MULTICAST_JOIN,
    MULTICAST_SEND,
    MULTICAST_QUIT,
    FILE_REQUEST,
    FILE_ACCEPT,
    FILE_REJECT,
    FILE_SEND,
    FILE_ACK
};

struct message {
    int pld_len;
    char nick_sender[NICK_LEN];
    enum msg_type type;
    char infos[INFOS_LEN];
};

char *msg_type_str[] = {
    "NICKNAME_NEW", "NICKNAME_LIST", "NICKNAME_INFOS", "ECHO_SEND",
    "UNICAST_SEND", "BROADCAST_SEND", "MULTICAST_CREATE", "MULTICAST_LIST",
    "MULTICAST_JOIN", "MULTICAST_SEND", "MULTICAST_QUIT", "FILE_REQUEST",
    "FILE_ACCEPT", "FILE_REJECT", "FILE_SEND", "FILE_ACK"
};

#include <stddef.h>

void die(int val, char *msg);
int read_from_socket(int fd, void *buf, size_t msg_size);
int write_in_socket(int fd, const void *buf, size_t msg_size);
int send_message(int fd, const struct message *msg, const char *payload);
int recv_message(int fd, struct message *msg, char *payload, size_t payload_cap);

#endif
