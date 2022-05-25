
#include <sys/time.h>
#include <unistd.h>
#include "redis-migrate.h"
#include "hiredis.h"
#include "async.h"
#include "ae.h"

static redisAsyncContext *context;
char *bind_source_addr;
static aeEventLoop *loop;

int sync_state;

RedisModuleCtx *moduleCtx;

/* Return the UNIX time in microseconds */
long long ustime(void) {
    struct timeval tv;
    long long ust;

    gettimeofday(&tv, NULL);
    ust = ((long long) tv.tv_sec) * 1000000;
    ust += tv.tv_usec;
    return ust;
}

/* Return the UNIX time in milliseconds */
mstime_t mstime(void) {
    return ustime() / 1000;
}

void syncWithMaster() {

}

void connectCallback(const redisAsyncContext *c, int status) {

}

void disconnectCallback(const redisAsyncContext *c, int status) {

}

int connectWithSourceRedis(RedisModuleCtx *ctx, robj *host, robj *p) {
    int port = atoi(p->ptr);
    context = redisAsyncConnect(host->ptr, port);
    if (context->err) {
        RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_WARNING, "Could not connect to Redis at ip:%s,port:%s",
                        (char *) host->ptr,
                        (char *) p->ptr);
        goto err;
    }
    loop = aeCreateEventLoop(64);
    redisAeAttach(loop, context);
    redisAsyncSetConnectCallback(context, connectCallback);
    redisAsyncSetDisconnectCallback(context, disconnectCallback);
    // send commands
    return C_OK;
    err:
    redisAsyncFree((redisAsyncContext *) context);
    context = NULL;

    if (loop != NULL) {
        aeStop(loop);
        loop = NULL;
    }
    return C_ERR;
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
    robj *port = (robj *) argv[2];
    robj *begin_slot = (robj *) argv[3];
    robj *end_slot = (robj *) argv[4];
    RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_NOTICE, "host:%s, port:%s, begin:%s, end:%s", (char *) host->ptr,
                    (char *) port->ptr, (char *) begin_slot->ptr, (char *) end_slot->ptr);
    if (context != NULL) {
        return RedisModule_ReplyWithError(ctx, "-ERR is migrating, please waiting");
    }
    if (connectWithSourceRedis(ctx, host, port) == C_ERR) {
        return RedisModule_ReplyWithError(ctx, "-ERR Can't connect source");
    }
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
    moduleCtx = ctx;
    return REDISMODULE_OK;
}
