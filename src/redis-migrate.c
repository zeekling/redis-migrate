
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include "redis-migrate.h"
#include "hiredis.h"
#include "async.h"

static redisContext *context;

void *syncWithRedis(void *arg) {
    RedisModuleCtx *ctx = arg;
    RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_NOTICE, "begin sync data");
    return NULL;
}
/**
 * migrate data to current instance.
 * migrate host port begin-slot end-slot
 * @param ctx
 * @param argv
 * @param argc
 * @return
 */
int rm_migrateCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 5) {
        return RedisModule_WrongArity(ctx);
    }
    robj *host = (robj *) argv[1];
    robj *p = (robj *) argv[2];
    robj *begin_slot = (robj *) argv[3];
    robj *end_slot = (robj *) argv[4];
    RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_NOTICE, "host:%s, port:%s, begin:%s, end:%s", (char *) host->ptr,
                    (char *) p->ptr, (char *) begin_slot->ptr, (char *) end_slot->ptr);
    if (context != NULL) {
        return RedisModule_ReplyWithError(ctx, "-ERR is migrating, please waiting");
    }
    int port = atoi(p->ptr);
    struct timeval timeout = {0, 500000}; // 0.5s
    context = redisConnectWithTimeout(host->ptr, port, timeout);
    if (context == NULL || context->err) {
        RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_WARNING, "Could not connect to Redis at ip:%s,port:%s",
                        (char *) host->ptr,
                        (char *) p->ptr);
        redisFree((redisContext *) context);
        context = NULL;
        return RedisModule_ReplyWithError(ctx, "Can't connect source redis");
    }

    pthread_t pthread;
    int flag = pthread_create(&pthread, NULL, syncWithRedis, ctx);
    if (flag == 0) {
        RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_WARNING, "Can't start thread");
        redisFree((redisContext *) context);
        context = NULL;
        return RedisModule_ReplyWithError(ctx, "Can't start thread");
    }
    pthread_join(pthread, NULL);
    return RedisModule_ReplyWithSimpleString(ctx, "OK");
}

/* This function must be present on each Redis module. It is used in order to
 * register the commands into the Redis server. */
int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {

    REDISMODULE_NOT_USED(argv);
    REDISMODULE_NOT_USED(argc);

    int flag = RedisModule_Init(ctx, MODULE_NAME, REDIS_MIGRATE_VERSION, REDISMODULE_APIVER_1);
    if (flag == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }
    RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_NOTICE, "init %s success", MODULE_NAME);
    flag = RedisModule_CreateCommand(ctx, "rm.migrate", rm_migrateCommand, "write deny-oom admin", 1, 1, 0);
    if (flag == REDISMODULE_ERR) {
        RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_WARNING, "init rm.migrate failed");
        return REDISMODULE_ERR;
    }
    return REDISMODULE_OK;
}
