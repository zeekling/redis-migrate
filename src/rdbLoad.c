#include "rdbLoad.h"
#include <errno.h>

int rdbLoadRioWithLoading(migrateObj *mobj)
{
    char buf[1024];
    int error;
    if (syncReadLine(mobj->source_cc->fd, buf, 9, mobj->timeout) == -1)
    {
        serverLog(LL_WARNING, "read version failed:%s,port:%d ", mobj->host, mobj->port);
        return C_ERR;
    }
    buf[9] = '\0';
    if (memcmp(buf, "REDIS", 5) != 0)
    {
        serverLog(LL_WARNING, "Wrong signature trying to load DB from file");
        errno = EINVAL;
        return C_ERR;
    }
    int type, rdbver;
    rdbver = atoi(buf + 5);
    if (rdbver < 1 || rdbver > RDB_VERSION)
    {
        serverLog(LL_WARNING, "Can't handle RDB format version %d", rdbver);
        errno = EINVAL;
        return C_ERR;
    }
    serverLog(LL_NOTICE, "buf=%s", buf);
    long long lru_idle = -1, lfu_freq = -1, expiretime = -1, now = mstime();
    while (1)
    {
        sds key;
        robj *val;
        if ((type = rdbLoadType(mobj)) == -1) {
            serverLog(LL_WARNING, "read type failed");
            return C_ERR;
        }
        if (type == RDB_OPCODE_EXPIRETIME) {
            
        }
    }
}


int rdbLoadType(migrateObj *mobj) {
    char buf[1];
    syncReadLine(mobj->source_cc->fd, buf, 1, mobj->timeout);
    return buf[0];
}

time_t rdbLoadTime(migrateObj *mobj) {
    
}