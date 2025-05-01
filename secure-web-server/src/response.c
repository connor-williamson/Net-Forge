#include <string.h>
#include <sys/socket.h>
#include "response.h"

#ifdef USE_SSL
#include <openssl/ssl.h>
#endif

void send_404_tcp(int client_fd) {
    const char *not_found =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n\r\n"
        "<html><body><h1>404 Not Found</h1></body></html>";
    send(client_fd, not_found, strlen(not_found), 0);
}

#ifdef USE_SSL
void send_404_ssl(SSL *ssl) {
    const char *not_found =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n\r\n"
        "<html><body><h1>404 Not Found</h1></body></html>";
    SSL_write(ssl, not_found, strlen(not_found));
}
#endif