#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "logger.h"

void log_request(const char *logfile, const char *ip, const char *path, int status_code, long response_time_ms, const char *user_agent) {
    mkdir("logs", 0755);
    FILE *log = fopen(logfile, "a");
    if (log) {
        time_t now = time(NULL);
        char *timestr = ctime(&now);
        timestr[strlen(timestr) - 1] = '\0';
        fprintf(log, "[%s] %s requested %s - %d (%ldms) UA: %s\n",
                timestr,
                ip ? ip : "UNKNOWN",
                path,
                status_code,
                response_time_ms,
                user_agent ? user_agent : "UNKNOWN");
        fclose(log);
    }
}
