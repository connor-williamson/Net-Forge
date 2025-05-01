#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>  // for mkdir()
#include "logger.h"

void log_request(const char *logfile, const char *ip, const char *path) {
    mkdir("logs", 0755);
    FILE *log = fopen(logfile, "a");
    if (log) {
        time_t now = time(NULL);
        char *timestr = ctime(&now);
        timestr[strlen(timestr) - 1] = '\0';
        fprintf(log, "[%s] %s requested %s\n", timestr, ip ? ip : "UNKNOWN", path);
        fclose(log);
    }
}
