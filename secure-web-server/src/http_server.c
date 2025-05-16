#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "queue.h"
#include "mime.h"
#include "logger.h"
#include "response.h"

#define PORT 8080
#define BACKLOG 10
#define BUFFER_SIZE 2048
#define ROOT_DIR "public_html"
#define HTTP_LOG_FILE "logs/http_access.log"

#define THREAD_POOL_SIZE 4
#define QUEUE_CAPACITY 32

volatile sig_atomic_t shutdown_requested = 0;
int server_fd = -1;
queue_t connection_queue;
pthread_t workers[THREAD_POOL_SIZE];
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void handle_shutdown(int signum) {
    (void)signum;
    shutdown_requested = 1;
    printf("\nGraceful shutdown requested.\n");
    queue_shutdown(&connection_queue); 
}

char *extract_user_agent(const char *buffer) {
    const char *ua_start = strstr(buffer, "User-Agent:");
    if (!ua_start) return NULL;
    ua_start += strlen("User-Agent:");
    while (*ua_start == ' ') ua_start++;
    const char *ua_end = strstr(ua_start, "\r\n");
    if (!ua_end) return NULL;
    size_t len = ua_end - ua_start;
    char *ua = malloc(len + 1);
    strncpy(ua, ua_start, len);
    ua[len] = '\0';
    return ua;
}

long get_time_diff_ms(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec) * 1000L + (end.tv_usec - start.tv_usec) / 1000L;
}

void handle_client(int client_fd) {
    printf("Handling connection in thread...\n");
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    getpeername(client_fd, (struct sockaddr *)&client_addr, &addr_len);
    struct timeval timeout = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    if (shutdown_requested) {
        close(client_fd);
        return;
    }
    while ((bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        char *user_agent = extract_user_agent(buffer);
        char *request = strtok(buffer, "\r\n\r\n");
        while (request != NULL) {
            if (strncmp(request, "Host:", 5) == 0 ||
                strncmp(request, "User-Agent:", 11) == 0 ||
                strncmp(request, "Accept:", 7) == 0 ||
                strncmp(request, "Connection:", 11) == 0 ||
                strlen(request) == 0) {
                request = strtok(NULL, "\r\n\r\n");
                continue;
            }
            struct timeval start, end;
            gettimeofday(&start, NULL);
            char method[8] = {0};
            char path[1024] = {0};
            if (sscanf(request, "%7s %1023s", method, path) != 2 ||
                strcmp(method, "GET") != 0 ||
                strstr(path, "..") || strchr(path, '%') || strchr(path, '\\') ||
                strlen(path) > 512 || strstr(request, " HTTP/") == NULL) {
                send_404_tcp(client_fd, 1);
                gettimeofday(&end, NULL);
                pthread_mutex_lock(&log_mutex);
                log_request(HTTP_LOG_FILE, inet_ntoa(client_addr.sin_addr), path, 404,
                            get_time_diff_ms(start, end), user_agent);
                pthread_mutex_unlock(&log_mutex);
                request = strtok(NULL, "\r\n\r\n");
                continue;
            }
            char full_path[2048];
            if (strcmp(path, "/") == 0) {
                snprintf(full_path, sizeof(full_path), "%s/index.html", ROOT_DIR);
            } else {
                snprintf(full_path, sizeof(full_path), "%s%s", ROOT_DIR, path);
            }
            size_t content_length = 0;
            int status = send_file(client_fd, full_path, &content_length) ? 200 : 404;
            gettimeofday(&end, NULL);
            pthread_mutex_lock(&log_mutex);
            log_request(HTTP_LOG_FILE, inet_ntoa(client_addr.sin_addr), path, status,
                        get_time_diff_ms(start, end), user_agent);
            pthread_mutex_unlock(&log_mutex);
            request = strtok(NULL, "\r\n\r\n");
        }
        free(user_agent);
        if (shutdown_requested) break;
    }
    if (bytes_received < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
        printf("Client recv() timed out.\n");
    }
    close(client_fd);
    printf("Thread done with connection.\n");
}

void *worker_thread(void *arg) {
    (void)arg;
    int client_fd;
    while (queue_dequeue(&connection_queue, &client_fd)) {
        handle_client(client_fd);
    }
    return NULL;
}

int main() {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int opt = 1;
    signal(SIGINT, handle_shutdown);
    queue_init(&connection_queue, QUEUE_CAPACITY);
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
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
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_create(&workers[i], NULL, worker_thread, NULL);
    }
    while (!shutdown_requested) {
        fd_set fds;
        struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };
        FD_ZERO(&fds);
        FD_SET(server_fd, &fds);
        int ready = select(server_fd + 1, &fds, NULL, NULL, &timeout);
        if (ready == -1) {
            if (errno == EINTR) continue;
            perror("select failed");
            break;
        }
        if (ready == 0) continue;
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (shutdown_requested) break;
            perror("accept failed");
            continue;
        }
        if (!queue_enqueue(&connection_queue, client_fd)) {
            fprintf(stderr, "Server too busy, dropping connection.\n");
            close(client_fd);
        }
    }
    close(server_fd);
    queue_shutdown(&connection_queue);
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_join(workers[i], NULL);
    }
    queue_destroy(&connection_queue);
    pthread_mutex_destroy(&log_mutex);
    printf("Server shutdown cleanly.\n");
    return 0;
}
