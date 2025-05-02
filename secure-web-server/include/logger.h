#ifndef LOGGER_H
#define LOGGER_H
void log_request(const char *logfile, const char *ip, const char *path, int status_code, long response_time_ms, const char *user_agent);
#endif
