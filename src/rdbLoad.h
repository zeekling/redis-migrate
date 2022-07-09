#ifndef RDB_LOAD_REDIS_MIGRATE_H
#define RDB_LOAD_REDIS_MIGRATE_H
#include "redis-migrate.h"

#define RDB_6BITLEN 0
#define RDB_14BITLEN 1
#define RDB_32BITLEN 0x80
#define RDB_64BITLEN 0x81
#define RDB_ENCVAL 3
#define RDB_LENERR UINT64_MAX

#define RDB_VERSION 10
#define RDB_OPCODE_FUNCTION2 245     /* function library data */
#define RDB_OPCODE_FUNCTION 246      /* old function library data for 7.0 rc1 and rc2 */
#define RDB_OPCODE_MODULE_AUX 247    /* Module auxiliary data. */
#define RDB_OPCODE_IDLE 248          /* LRU idle time. */
#define RDB_OPCODE_FREQ 249          /* LFU frequency. */
#define RDB_OPCODE_AUX 250           /* RDB aux field. */
#define RDB_OPCODE_RESIZEDB 251      /* Hash table resize hint. */
#define RDB_OPCODE_EXPIRETIME_MS 252 /* Expire time in milliseconds. */
#define RDB_OPCODE_EXPIRETIME 253    /* Old expire time in seconds. */
#define RDB_OPCODE_SELECTDB 254      /* DB number of the following keys. */
#define RDB_OPCODE_EOF 255           /* End of the RDB file. */

#define RDB_LOAD_NONE 0
#define RDB_LOAD_ENC (1 << 0)
#define RDB_LOAD_PLAIN (1 << 1)
#define RDB_LOAD_SDS (1 << 2)

#define RDB_ENC_INT8 0        /* 8 bit signed integer */
#define RDB_ENC_INT16 1       /* 16 bit signed integer */
#define RDB_ENC_INT32 2       /* 32 bit signed integer */
#define RDB_ENC_LZF 3         /* string compressed with FASTLZ */

#define LONG_STR_SIZE      21
extern const char *SDS_NOINIT;
#define OBJ_STRING 0    /* String object. */
#define OBJ_LIST 1      /* List object. */
#define OBJ_SET 2       /* Set object. */
#define OBJ_ZSET 3      /* Sorted set object. */
#define OBJ_HASH 4      /* Hash object. */

#define htonu64(v) intrev64(v)
#define ntohu64(v) intrev64(v)

#define LLONG_MAX __LONG_LONG_MAX__

int rmRead(migrateObj *mobj, void *buf, size_t len);

int rmLoadRioWithLoading(migrateObj *mobj);

int rmLoadType(migrateObj *mobj);

time_t rmLoadTime(migrateObj *mobj);

long long rmLoadMillisecondTime(migrateObj *mobj, int rdbver);

int rmLoadLenByRef(migrateObj *mobi, int *isencoded, uint64_t *lenptr);

void rm_memrev64(void *p);

uint64_t rmLoadLen(migrateObj *mobj, int *isencoded);

robj *rmLoadStringObject(migrateObj *mobj);

void *rmGenericLoadStringObject(migrateObj *mobj, int flags, size_t *lenptr);

void *rmLoadIntegerObject(migrateObj *mobj, int enctype, int flags, size_t *lenptr);

void *rmLoadLzfStringObject(migrateObj *mobj, int flags, size_t *lenptr);

robj *rmLoadObject(int rdbtype, migrateObj *mobj, sds key, int *error);

#endif
