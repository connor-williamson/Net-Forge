#ifndef RESPONSE_H
#define RESPONSE_H
#include <openssl/ssl.h>
#include <stddef.h>
int send_file(int client_fd, const char *filepath, size_t *content_length);
void send_404_tcp(int client_fd, int keep_alive);
void send_404_ssl(SSL *ssl);
#endif