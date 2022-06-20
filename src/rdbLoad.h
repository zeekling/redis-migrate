#ifndef RDB_LOAD_REDIS_MIGRATE_H
#define RDB_LOAD_REDIS_MIGRATE_H
#include "redis-migrate.h"


#define RDB_VERSION 10
#define RDB_OPCODE_FUNCTION2  245   /* function library data */
#define RDB_OPCODE_FUNCTION   246   /* old function library data for 7.0 rc1 and rc2 */
#define RDB_OPCODE_MODULE_AUX 247   /* Module auxiliary data. */
#define RDB_OPCODE_IDLE       248   /* LRU idle time. */
#define RDB_OPCODE_FREQ       249   /* LFU frequency. */
#define RDB_OPCODE_AUX        250   /* RDB aux field. */
#define RDB_OPCODE_RESIZEDB   251   /* Hash table resize hint. */
#define RDB_OPCODE_EXPIRETIME_MS 252    /* Expire time in milliseconds. */
#define RDB_OPCODE_EXPIRETIME 253       /* Old expire time in seconds. */
#define RDB_OPCODE_SELECTDB   254   /* DB number of the following keys. */
#define RDB_OPCODE_EOF        255   /* End of the RDB file. */

int rdbLoadRioWithLoading(migrateObj *mobj);

int rdbLoadType(migrateObj *mobj);

time_t rdbLoadTime(migrateObj *mobj);

#endif
