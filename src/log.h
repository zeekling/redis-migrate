
#ifndef REDIS_MIGRATE_LOG_H
#define REDIS_MIGRATE_LOG_H

#define NOTICE "notice"
#define WARNING "warning"
#define VERBOSE "VERBOSE"
#define DEBUG "debug"

#define LL_DEBUG 0
#define LL_VERBOSE 1
#define LL_NOTICE 2
#define LL_WARNING 3
#define LOG_MAX_LEN    1024

typedef struct logObj {
    char *logfile;
    int loglevel;
} logObj;

static logObj *log;

void createLogObj(char *logfile);

#define serverLog(level, ...) _serverLog(level, __VA_ARGS__);

void _serverLog(int level, const char *fmt, ...)
__attribute__((format(printf, 2, 3)));

void serverLogRaw(int level, const char *msg);

void nolocks_localtime(struct tm *tmp, time_t t, time_t tz, int dst);

#endif //REDIS_MIGRATE_LOG_H
