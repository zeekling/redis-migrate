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

#define LLONG_MAX __LONG_LONG_MAX__

int rm_rdbLoadRioWithLoading(migrateObj *mobj);

int rm_rdbLoadType(migrateObj *mobj);

time_t rm_rdbLoadTime(migrateObj *mobj);

long long rm_rdbLoadMillisecondTime(migrateObj *mobj, int rdbver);

uint64_t rm_rdbLoadLen(migrateObj *mobi, int *isencoded);

int rm_rdbLoadLenByRef(migrateObj *mobi, int *isencoded, uint64_t *lenptr);

void rm_memrev64(void *p);

uint64_t rm_rdbLoadLen(migrateObj *mobj, int *isencoded);

void *rm_rdbGenericLoadStringObject(migrateObj *mobj, int flags, size_t *lenptr);

#endif
