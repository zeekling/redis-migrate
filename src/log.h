
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
    const char *logfile;
    int loglevel;
} logObj;

#define migrateLog(mLog, level, ...) _migrateLog(mLog, level, __VA_ARGS__);

void _migrateLog(logObj mLog, int level, const char *fmt, ...)
__attribute__((format(printf, 3, 4)));

void migrateLogRaw(logObj mLog, int level, const char *msg);

void nolocks_localtime(struct tm *tmp, time_t t, time_t tz, int dst);

#endif //REDIS_MIGRATE_LOG_H
