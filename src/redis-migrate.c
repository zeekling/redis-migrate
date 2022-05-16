
#include "redis-migrate.h"

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
