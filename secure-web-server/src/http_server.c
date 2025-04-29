#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/time.h>
#include "utils.h"

#define PORT 8080
#define BUFFER_SIZE 2048
#define BACKLOG 5
#define ROOT_DIR "public_html"
#define HTTP_LOG_FILE "logs/http_access.log"

void send_file(int client_fd, const char *filepath) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        perror("fopen");
        send_404_tcp(client_fd);
        return;
    }

    const char *mime = get_mime_type(filepath);
    char header[256];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Connection: close\r\n\r\n", mime);
    send(client_fd, header, strlen(header), 0);

    char file_buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(file_buffer, 1, BUFFER_SIZE, fp)) > 0) {
        send(client_fd, file_buffer, bytes_read, 0);
    }

    fclose(fp);
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    int opt = 1;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("HTTP server listening on port %d...\n", PORT);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept failed");
            continue;
        }

        struct timeval timeout = {5, 0};
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);

        if (bytes_received <= 0) {
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
            send_404_tcp(client_fd);
            close(client_fd);
            continue;
        }

        log_request(HTTP_LOG_FILE, inet_ntoa(client_addr.sin_addr), path);

        char full_path[2048];
        if (strcmp(path, "/") == 0) {
            snprintf(full_path, sizeof(full_path), "%s/index.html", ROOT_DIR);
        } else {
            snprintf(full_path, sizeof(full_path), "%s%s", ROOT_DIR, path);
        }

        send_file(client_fd, full_path);
        close(client_fd);
    }

    close(server_fd);
    return 0;
}

