
#ifndef REDIS_MIGRATE_REDIS_MIGRATE_H
#define REDIS_MIGRATE_REDIS_MIGRATE_H

#include "redismodule.h"
#include "ae.h"
#include "sds.h"
#include "sdscompat.h"

#define MODULE_NAME "redis-migrate"
#define REDIS_MIGRATE_VERSION 1
#define LRU_BITS 24
#define C_ERR -1
#define C_OK 1

/* Anti-warning macro... */
#define UNUSED(V) ((void) V)

typedef struct redisObject {
    unsigned type: 4;
    unsigned encoding: 4;
    unsigned lru: LRU_BITS; /* LRU time (relative to global lru_clock) or
                            * LFU data (least significant 8 bits frequency
                            * and most significant 16 bits access time). */
    int refcount;
    void *ptr;
} robj;

typedef struct migrateObject {
    char *address;
    int repl_stat;
    redisContext *source_cc;
    char *host;
    int port;
    int begin_slot;
    int end_slot;
    char *psync_replid;
    int isCache;
    char psync_offset[32];
} migrateObj;

typedef enum {
    REPL_STATE_NONE = 0,            /* No active replication */
    REPL_STATE_CONNECT,             /* Must connect to master */
    REPL_STATE_CONNECTING,          /* Connecting to master */
    /* --- Handshake states, must be ordered --- */
    REPL_STATE_RECEIVE_PING_REPLY,  /* Wait for PING reply */
    REPL_STATE_SEND_HANDSHAKE,      /* Send handshake sequence to master */
    REPL_STATE_RECEIVE_AUTH_REPLY,  /* Wait for AUTH reply */
    REPL_STATE_RECEIVE_PORT_REPLY,  /* Wait for REPLCONF reply */
    REPL_STATE_RECEIVE_IP_REPLY,    /* Wait for REPLCONF reply */
    REPL_STATE_RECEIVE_CAPA_REPLY,  /* Wait for REPLCONF reply */
    REPL_STATE_SEND_PSYNC,          /* Send PSYNC */
    REPL_STATE_RECEIVE_PSYNC_REPLY, /* Wait for PSYNC reply */
    /* --- End of handshake states --- */
    REPL_STATE_TRANSFER,        /* Receiving .rdb from master */
    REPL_STATE_CONNECTED,       /* Connected to master */
    STATE_CONNECT_ERROR,
    STATE_DISCONNECT
} repl_state;

long long ustime(void);

migrateObj *createMigrateObject(robj *host, int port, int begin_slot, int end_slot);

void freeMigrateObj(migrateObj *m);

int sendSyncCommand(RedisModuleCtx *ctx);

void *syncWithRedis(void *arg);

int connectRedis(RedisModuleCtx *ctx) ;

int rm_migrateCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);

int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);

#endif //REDIS_MIGRATE_REDIS_MIGRATE_H
