#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>

#ifdef USE_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#define LOG_FILE "logs/http_access.log"

const char* get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "text/plain";
    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    return "application/octet-stream";
}

void log_request(const char *ip, const char *path) {
    FILE *log = fopen(LOG_FILE, "a");
    if (log) {
        time_t now = time(NULL);
        char *timestr = ctime(&now);
        timestr[strlen(timestr) - 1] = '\0';  
        fprintf(log, "[%s] %s requested %s\n", timestr, ip, path);
        fclose(log);
    }
}

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
