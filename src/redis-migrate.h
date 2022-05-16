
#ifndef REDIS_MIGRATE_REDIS_MIGRATE_H
#define REDIS_MIGRATE_REDIS_MIGRATE_H

#include "redismodule.h"

#define MODULE_NAME "redis-migrate"
#define REDIS_MIGRATE_VERSION 1
#define LRU_BITS 24

typedef struct redisObject {
    unsigned type:4;
    unsigned encoding:4;
    unsigned lru:LRU_BITS; /* LRU time (relative to global lru_clock) or
                            * LFU data (least significant 8 bits frequency
                            * and most significant 16 bits access time). */
    int refcount;
    void *ptr;
} robj;

int rm_migrateCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);
int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);

#endif //REDIS_MIGRATE_REDIS_MIGRATE_H
