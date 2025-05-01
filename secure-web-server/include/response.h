#ifndef RESPONSE_H
#define RESPONSE_H
#include <openssl/ssl.h>
void send_404_tcp(int client_fd);
void send_404_ssl(SSL *ssl);
#endif