
#ifndef REDIS_MIGRATE_REDIS_MIGRATE_H
#define REDIS_MIGRATE_REDIS_MIGRATE_H

#include <map>
#include <string>
#include "redismodule.h"
#include "hiredis/sds.h"
#include "log.h"

#define MODULE_NAME "redis-migrate"
#define REDIS_MIGRATE_VERSION 1
#define LRU_BITS 24
#define CONFIG_RUN_ID_SIZE 40
#define RDB_EOF_MARK_SIZE 40
#define PSYNC_WRITE_ERROR 0
#define PSYNC_WAIT_REPLY 1
#define PSYNC_CONTINUE 2
#define PSYNC_FULLRESYNC 3
#define PSYNC_NOT_SUPPORTED 4
#define PSYNC_TRY_LATER 5
#define C_ERR -1
#define C_OK 1
#define PROTO_IOBUF_LEN (1024 * 16)

#define MAX_MIGRATEING_NUM 50

/* Anti-warning macro... */
#define UNUSED(V) ((void)V)

typedef struct migrateObject {
    char *address;
    int migrating;
    const char *host;
    size_t hostLen = 0;
    size_t port = 0;
    size_t slot = 0;
    const char *key;
    size_t keyLen = 0;
    int timeout;
    int isCache;
} migrateObj;

static std::map<std::string, migrateObj> migrating;


migrateObj createMigrateObject(RedisModuleString *host, int port, int slot, RedisModuleString *key, migrateObj m);

int rm_migrateCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);

void rm_migrateFilter(RedisModuleCommandFilterCtx *filter);

#ifdef __cplusplus
extern "C" {
#endif
int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);
#ifdef __cplusplus
}
#endif
#endif // REDIS_MIGRATE_REDIS_MIGRATE_H
