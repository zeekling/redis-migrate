
#ifndef REDIS_MIGRATE_REDIS_MIGRATE_H
#define REDIS_MIGRATE_REDIS_MIGRATE_H

#include "redismodule.h"
#include "ae.h"
#include "sds.h"
#include "sdscompat.h"
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

/* Anti-warning macro... */
#define UNUSED(V) ((void)V)

typedef struct redisObject
{
    unsigned type : 4;
    unsigned encoding : 4;
    unsigned lru : LRU_BITS;
    int refcount;
    void *ptr;
} robj;

typedef struct migrateObject
{
    char *address;
    int repl_stat;
    redisContext *source_cc;
    char *host;
    int port;
    int begin_slot;
    int end_slot;
    char *psync_replid;
    char master_replid[CONFIG_RUN_ID_SIZE + 1];
    int timeout;
    int isCache;
    char psync_offset[32];
    int repl_transfer_size;
    long long master_initial_offset;
    time_t repl_transfer_lastio;
} migrateObj;

typedef enum
{
    REPL_STATE_NONE = 0,   /* No active replication */
    REPL_STATE_CONNECT,    /* Must connect to master */
    REPL_STATE_CONNECTING, /* Connecting to master */
    /* --- Handshake states, must be ordered --- */
    REPL_STATE_RECEIVE_PING_REPLY,  /* Wait for PING reply */
    REPL_STATE_SEND_HANDSHAKE,      /* Send handshake sequence to master */
    REPL_STATE_RECEIVE_AUTH_REPLY,  /* Wait for AUTH reply */
    REPL_STATE_RECEIVE_PORT_REPLY,  /* Wait for REPLCONF reply */
    REPL_STATE_RECEIVE_IP_REPLY,    /* Wait for REPLCONF reply */
    REPL_STATE_RECEIVE_CAPA_REPLY,  /* Wait for REPLCONF reply */
    REPL_STATE_SEND_PSYNC,          /* Send PSYNC */
    REPL_STATE_RECEIVE_PSYNC_REPLY, /* Wait for PSYNC reply */
    REPL_STATE_FULL_SYNC,
    REPL_STATE_CONTINUE_SYNC,
    /* --- End of handshake states --- */
    REPL_STATE_TRANSFER,  /* Receiving .rdb from master */
    REPL_STATE_CONNECTED, /* Connected to master */
} repl_state;

long long ustime(void);

mstime_t mstime(void);

migrateObj *createMigrateObject(robj *host, int port, int begin_slot, int end_slot);

void freeMigrateObj(migrateObj *m);

int sendSyncCommand();

int receiveDataFromRedis();

ssize_t syncWrite(int fd, char *ptr, ssize_t size, long long timeout);

ssize_t syncRead(int fd, char *ptr, ssize_t size, long long timeout);

ssize_t syncReadLine(int fd, char *ptr, ssize_t size, long long timeout);

sds redisReceive();

void readFullData();

void cancelMigrate();

void syncDataWithRedis(int fd, void *user_data, int mask);

int rm_migrateCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);

int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);

#endif // REDIS_MIGRATE_REDIS_MIGRATE_H
