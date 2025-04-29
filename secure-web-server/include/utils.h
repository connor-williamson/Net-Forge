#ifndef UTILS_H
#define UTILS_H

#include <openssl/ssl.h>

const char* get_mime_type(const char *path);
void log_request(const char *logfile, const char *ip, const char *path);
void send_404_tcp(int client_fd);
void send_404_ssl(SSL *ssl);
#endif
