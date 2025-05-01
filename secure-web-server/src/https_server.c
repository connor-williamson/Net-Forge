#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/time.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "mime.h"
#include "logger.h"
#include "response.h"

#define PORT 4433
#define BUFFER_SIZE 2048
#define BACKLOG 5
#define ROOT_DIR "public_html"
#define CERT_FILE "certs/cert.pem"
#define KEY_FILE "certs/key.pem"
#define HTTPS_LOG_FILE "logs/https_access.log"

SSL_CTX *initialize_ssl_context() {
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    const SSL_METHOD *method = TLS_server_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}

void configure_ssl_context(SSL_CTX *ctx) {
    if (SSL_CTX_use_certificate_file(ctx, CERT_FILE, SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, KEY_FILE, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
}

void send_file_ssl(SSL *ssl, const char *filepath) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        perror("fopen");
        send_404_ssl(ssl);
        return;
    }

    const char *mime = get_mime_type(filepath);
    char header[512];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Connection: close\r\n\r\n", mime);
    SSL_write(ssl, header, strlen(header));

    char file_buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(file_buffer, 1, BUFFER_SIZE, fp)) > 0) {
        SSL_write(ssl, file_buffer, bytes_read);
    }

    fclose(fp);
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    SSL_CTX *ctx = initialize_ssl_context();
    configure_ssl_context(ctx);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("HTTPS server listening on port %d...\n", PORT);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept failed");
            continue;
        }

        struct timeval timeout = {5, 0};
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        SSL *ssl = SSL_new(ctx);
        SSL_set_fd(ssl, client_fd);

        if (SSL_accept(ssl) <= 0) {
            ERR_print_errors_fp(stderr);
            SSL_free(ssl);
            close(client_fd);
            continue;
        }

        char buffer[BUFFER_SIZE] = {0};
        ssize_t bytes_received = SSL_read(ssl, buffer, BUFFER_SIZE - 1);

        if (bytes_received <= 0) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
            close(client_fd);
            continue;
        }

        buffer[bytes_received] = '\0';

        char method[8] = {0};
        char path[1024] = {0};
        if (sscanf(buffer, "%7s %1023s", method, path) != 2 ||
            strncmp(method, "GET", 3) != 0 ||
            strstr(path, "..") || strchr(path, '%') || strchr(path, '\\') ||
            strlen(path) > 512 || strstr(buffer, " HTTP/") == NULL) {
            send_404_ssl(ssl);
            SSL_shutdown(ssl);
            SSL_free(ssl);
            close(client_fd);
            continue;
        }

        log_request(HTTPS_LOG_FILE, inet_ntoa(client_addr.sin_addr), path);

        char full_path[2048];
        if (strcmp(path, "/") == 0) {
            snprintf(full_path, sizeof(full_path), "%s/index.html", ROOT_DIR);
        } else {
            snprintf(full_path, sizeof(full_path), "%s%s", ROOT_DIR, path);
        }

        send_file_ssl(ssl, full_path);

        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client_fd);
    }

    close(server_fd);
    SSL_CTX_free(ctx);
    return 0;
}
