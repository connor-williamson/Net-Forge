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

int send_file_ssl(SSL *ssl, const char *filepath, size_t *content_length) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        perror("fopen");
        send_404_ssl(ssl);
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
             "Connection: close\r\n\r\n", mime, *content_length);
    SSL_write(ssl, header, strlen(header));

    char file_buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(file_buffer, 1, BUFFER_SIZE, fp)) > 0) {
        SSL_write(ssl, file_buffer, bytes_read);
    }

    fclose(fp);
    return 1;
}

char *extract_user_agent(const char *buffer) {
    const char *ua_start = strstr(buffer, "User-Agent:");
    if (!ua_start) return NULL;

    ua_start += strlen("User-Agent:");
    while (*ua_start == ' ') ua_start++;
    const char *ua_end = strstr(ua_start, "\r\n");
    if (!ua_end) return NULL;

    size_t len = ua_end - ua_start;
    char *user_agent = malloc(len + 1);
    strncpy(user_agent, ua_start, len);
    user_agent[len] = '\0';
    return user_agent;
}

long get_time_diff_ms(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec) * 1000L + (end.tv_usec - start.tv_usec) / 1000L;
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

        struct timeval start, end;
        gettimeofday(&start, NULL);

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
        char *user_agent = extract_user_agent(buffer);

        if (sscanf(buffer, "%7s %1023s", method, path) != 2 ||
            strncmp(method, "GET", 3) != 0 ||
            strstr(path, "..") || strchr(path, '%') || strchr(path, '\\') ||
            strlen(path) > 512 || strstr(buffer, " HTTP/") == NULL) {
            send_404_ssl(ssl);
            gettimeofday(&end, NULL);
            log_request(HTTPS_LOG_FILE, inet_ntoa(client_addr.sin_addr), path, 404, get_time_diff_ms(start, end), user_agent);
            free(user_agent);
            SSL_shutdown(ssl);
            SSL_free(ssl);
            close(client_fd);
            continue;
        }

        char full_path[2048];
        if (strcmp(path, "/") == 0) {
            snprintf(full_path, sizeof(full_path), "%s/index.html", ROOT_DIR);
        } else {
            snprintf(full_path, sizeof(full_path), "%s%s", ROOT_DIR, path);
        }

        size_t content_length = 0;
        int status = send_file_ssl(ssl, full_path, &content_length) ? 200 : 404;

        gettimeofday(&end, NULL);
        log_request(HTTPS_LOG_FILE, inet_ntoa(client_addr.sin_addr), path, status, get_time_diff_ms(start, end), user_agent);
        free(user_agent);

        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client_fd);
    }

    close(server_fd);
    SSL_CTX_free(ctx);
    return 0;
}
