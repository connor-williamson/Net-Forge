#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include "response.h"
#include "mime.h"

#ifdef USE_SSL
#include <openssl/ssl.h>
#endif

#define BUFFER_SIZE 2048

void send_404_tcp(int client_fd, int keep_alive) {
    const char *template =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/html\r\n"
        "Connection: %s\r\n\r\n"
        "<html><body><h1>404 Not Found</h1></body></html>";
    char buffer[512];
    snprintf(buffer, sizeof(buffer), template, keep_alive ? "keep-alive" : "close");
    send(client_fd, buffer, strlen(buffer), 0);
}

int send_file(int client_fd, const char *filepath, size_t *content_length) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        perror("fopen");
        return 0;
    }

    fseek(fp, 0, SEEK_END);
    *content_length = ftell(fp);
    rewind(fp);

    const char *mime = get_mime_type(filepath);
    char header[512];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "Connection: keep-alive\r\n\r\n",
             mime, *content_length);
    send(client_fd, header, strlen(header), 0);

    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        send(client_fd, buffer, bytes_read, 0);
    }

    fclose(fp);
    return 1;
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