
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
    m->isCache = 0;
    return m;
}

void freeMigrateObj(migrateObj *m) {
    redisFree(m->source_cc);
    hi_free(m);
    mobj = NULL;
}

int sendSyncCommand(RedisModuleCtx *ctx) {

    if (!mobj->isCache) {
        mobj->isCache = 1;
        mobj->psync_replid = "?";
        memcpy(mobj->psync_offset, "-1", 3);
    }
    if (redisAppendCommand(mobj->source_cc, "PSYNC %s %s", mobj->psync_replid, mobj->psync_offset) != REDIS_OK) {
        RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_WARNING,
                        "append PSYNC %s %s failed ip:%s,port:%d, ",
                        mobj->psync_replid, mobj->psync_offset, mobj->host, mobj->port);
        return 0;
    }
    if (redisFlush(mobj->source_cc) != REDIS_OK) {
        RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_WARNING,
                        "send PSYNC %s %s failed ip:%s,port:%d, ",
                        mobj->psync_replid, mobj->psync_offset, mobj->host, mobj->port);
        return 0;
    }
    RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_NOTICE, "PSYNC %s %s ",
                    mobj->psync_replid, mobj->psync_offset);
    return 1;
}

void *syncWithRedis(void *arg) {
    RedisModuleCtx *ctx = arg;
    redisReply *reply;
    RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_NOTICE, "begin sync data");
    reply = redisCommand(mobj->source_cc, "PING");
    if (reply->type == REDIS_REPLY_ERROR) {
        RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_WARNING, "send PING failed ip:%s,port:%d, error:%s",
                        mobj->host, mobj->port, reply->str);
        goto error;
    }
    RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_NOTICE, "PING ");
    freeReplyObject(reply);
    //todo auth with master
    sds portstr = sdsfromlonglong(mobj->port);
    reply = redisCommand(mobj->source_cc, "REPLCONF listening-port %s", portstr);
    if (reply->type == REDIS_REPLY_ERROR) {
        sdsfree(portstr);
        RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_WARNING, "REPLCONF listening-port %s failed ip:%s,port:%d, error:%s",
                        portstr, mobj->host, mobj->port, reply->str);
        goto error;
    }
    RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_NOTICE, "REPLCONF listening-port %s ", portstr);
    sdsfree(portstr);
    freeReplyObject(reply);
    reply = redisCommand(mobj->source_cc, "REPLCONF ip-address %s", mobj->host);
    if (reply->type == REDIS_REPLY_ERROR) {
        RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_WARNING, "REPLCONF ip-address %s failed ip:%s,port:%d, error:%s",
                        mobj->host, mobj->host, mobj->port, reply->str);
        goto error;
    }
    RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_NOTICE, "REPLCONF ip-address %s ", mobj->host);
    freeReplyObject(reply);
    reply = redisCommand(mobj->source_cc, "REPLCONF %s %s %s %s", "capa", "eof", "capa", "psync2");
    if (reply->type == REDIS_REPLY_ERROR) {
        RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_WARNING,
                        "REPLCONF capa eof capa psync2 failed ip:%s,port:%d, error:%s",
                        mobj->host, mobj->port, reply->str);
        goto error;
    }
    RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_NOTICE, "REPLCONF %s %s %s %s",
                    "capa", "eof", "capa", "psync2");
    freeReplyObject(reply);
    if (!sendSyncCommand(ctx)) {
        RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_WARNING,
                        "send PSYNC %s %s failed ip:%s,port:%d, ",
                        mobj->psync_replid, mobj->psync_offset, mobj->host, mobj->port);
        goto error;
    }

    return NULL;
    error:
    freeReplyObject(reply);
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
