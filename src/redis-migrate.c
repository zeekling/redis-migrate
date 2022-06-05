
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "redis-migrate.h"
#include "hiredis.h"

static migrateObj *mobj;

migrateObj *createMigrateObject(robj *host, int port, int begin_slot, int end_slot) {
    migrateObj *m;
    m = hi_malloc(sizeof(*m));
    m->host = host->ptr;
    m->port = port;
    m->begin_slot = begin_slot;
    m->end_slot = end_slot;
    m->repl_stat = REPL_STATE_NONE;
    return m;
}

void freeMigrateObj(migrateObj *m) {
    redisFree(m->source_cc);
    hi_free(m);
    mobj = NULL;
}

int sendReplCommand(RedisModuleCtx *ctx, char *format, ...) {
    va_list ap;
    va_start(ap, format);
    redisReply *reply = redisvCommand(mobj->source_cc, format, ap);
    va_end(ap);
    if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
        RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_WARNING, "send %s failed ip:%s,port:%d, error:%s",
                        format, mobj->host, mobj->port, reply->str);
        freeReplyObject(reply);
        return 0;
    }
    RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_NOTICE, "%s %s", format, reply->str);
    freeReplyObject(reply);
    return 1;
}

void *syncWithRedis(void *arg) {
    RedisModuleCtx *ctx = arg;
    RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_NOTICE, "begin sync data");
    if (!sendReplCommand(ctx, "PING")) {
        goto error;
    }
    //todo auth with master
    sds portstr = sdsfromlonglong(mobj->port);
    if (!sendReplCommand(ctx, "REPLCONF listening-port %s", portstr)) {
        sdsfree(portstr);
        goto error;
    }
    sdsfree(portstr);
    if (!sendReplCommand(ctx, "REPLCONF ip-address %s", mobj->host)) {
        goto error;
    }
    if (!sendReplCommand(ctx, "REPLCONF %s %s %s %s", "capa", "eof", "capa", "psync2")) {
        goto error;
    }

    return NULL;
    error:
    freeMigrateObj(mobj);
    return NULL;
}

int connectRedis(RedisModuleCtx *ctx) {
    pthread_t pthread;
    int flag = pthread_create(&pthread, NULL, syncWithRedis, ctx);
    if (flag != 0) {
        RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_WARNING, "Can't start thread");
        freeMigrateObj(mobj);
        return RedisModule_ReplyWithError(ctx, "Can't start thread");
    }
    pthread_join(pthread, NULL);
    return RedisModule_ReplyWithSimpleString(ctx, "OK");
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
    if (mobj != NULL) {
        return RedisModule_ReplyWithError(ctx, "-ERR is migrating, please waiting");
    }
    mobj = createMigrateObject(host, atoi(port->ptr), atoi(begin_slot->ptr), atoi(end_slot->ptr));
    struct timeval timeout = {1, 500000}; // 1.5s
    redisOptions options = {0};
    REDIS_OPTIONS_SET_TCP(&options, (const char *) mobj->host, mobj->port);
    options.connect_timeout = &timeout;
    mobj->source_cc = redisConnectWithOptions(&options);
    if (mobj->source_cc == NULL || mobj->source_cc->err) {
        RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_WARNING, "Could not connect to Redis at ip:%s,port:%d, error:%s",
                        mobj->host, mobj->port, mobj->source_cc->errstr);
        freeMigrateObj(mobj);
        return RedisModule_ReplyWithError(ctx, "Can't connect source redis");
    }
    return connectRedis(ctx);

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
    RedisModule_SetClusterFlags(ctx, REDISMODULE_CLUSTER_FLAG_NO_REDIRECTION);
    RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_NOTICE, "begin init commands of %s", MODULE_NAME);
    flag = RedisModule_CreateCommand(ctx, "rm.migrate", rm_migrateCommand, "write deny-oom admin", 1, 1, 0);
    if (flag == REDISMODULE_ERR) {
        RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_WARNING, "init rm.migrate failed");
        return REDISMODULE_ERR;
    }
    RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_NOTICE, "init %s success", MODULE_NAME);
    return REDISMODULE_OK;
}
