#include "rdbLoad.h"

#include <errno.h>
#include <unistd.h>

int rdbLoadRioWithLoading(migrateObj *mobj) {
    char buf[1024];
    int error;
    if (syncReadLine(mobj->source_cc->fd, buf, 9, mobj->timeout) == -1) {
        serverLog(LL_WARNING, "read version failed:%s,port:%d ", mobj->host,
                  mobj->port);
        return C_ERR;
    }
    buf[9] = '\0';
    if (memcmp(buf, "REDIS", 5) != 0) {
        serverLog(LL_WARNING, "Wrong signature trying to load DB from file");
        errno = EINVAL;
        return C_ERR;
    }
    int type, rdbver;
    uint64_t dbid = 0;
    rdbver = atoi(buf + 5);
    if (rdbver < 1 || rdbver > RDB_VERSION) {
        serverLog(LL_WARNING, "Can't handle RDB format version %d", rdbver);
        errno = EINVAL;
        return C_ERR;
    }
    serverLog(LL_NOTICE, "buf=%s", buf);
    long long lru_idle = -1, lfu_freq = -1, expiretime = -1, now = mstime();
    while (1) {
        sds key;
        robj *val;
        if ((type = rm_rdbLoadType(mobj)) == -1) {
            serverLog(LL_WARNING, "read type failed");
            return C_ERR;
        }

        if (type == RDB_OPCODE_EXPIRETIME) {
            expiretime = rm_rdbLoadTime(mobj);
            if (expiretime == -1) {
                return C_ERR;
            }
            expiretime *= 1000;
            continue;
        } else if (type == RDB_OPCODE_EXPIRETIME_MS) {
            expiretime = rm_rdbLoadMillisecondTime(mobj, rdbver);
            continue;
        } else if (type == RDB_OPCODE_FREQ) {
            uint8_t byte;
            if (read(mobj->source_cc->fd, &byte, 1) == 0)
                return C_ERR;
            lfu_freq = byte;
            continue;
        } else if (type == RDB_OPCODE_IDLE) {
            uint64_t qword;
            if ((qword = rm_rdbLoadLen(mobj, NULL)) == RDB_LENERR)
                return C_ERR;
            lru_idle = qword;
            continue;
        } else if (type == RDB_OPCODE_EOF) {
            break;
        } else if (type == RDB_OPCODE_SELECTDB) {
            if ((dbid = rm_rdbLoadLen(mobj, NULL)) == RDB_LENERR) return C_ERR;
            continue;
        } else if (type == RDB_OPCODE_RESIZEDB) {
            uint64_t db_size, expires_size;
            if ((db_size = rm_rdbLoadLen(mobj, NULL)) == RDB_LENERR) return C_ERR;
            if ((expires_size = rm_rdbLoadLen(mobj, NULL)) == RDB_LENERR) return C_ERR;

            continue;
        } else if (type == RDB_OPCODE_AUX) {
            robj *auxkey, *auxval;

        } else if (type == RDB_OPCODE_FUNCTION || type == RDB_OPCODE_FUNCTION2) {
        }
    }
}

void *rm_rdbGenericLoadStringObject(migrateObj *mobj, int flags, size_t *lenptr) {
    int encode = flags & RDB_LOAD_ENC;
    int plain = flags & RDB_LOAD_PLAIN;
    int sds = flags & RDB_LOAD_SDS;
    int isencoded;
    unsigned long long len;
    
}

uint64_t rm_rdbLoadLen(migrateObj *mobj, int *isencoded) {
    uint64_t len;

    if (rdbLoadLenByRef(mobj, isencoded, &len) == -1)
        return RDB_LENERR;
    return len;
}

int rm_rdbLoadType(migrateObj *mobj) {
    unsigned char type;
    if (read(mobj->source_cc->fd, &type, 1) == 0) {
        return -1;
    }

    return type;
}

int rm_rdbLoadLenByRef(migrateObj *mobj, int *isencoded, uint64_t *lenptr) {
    unsigned char buf[2];
    int type;
    if (isencoded)
        *isencoded = 0;
    if (read(mobj->source_cc->fd, buf, 1) == 0) {
        return -1;
    }
    type = (buf[0] & 0xC0) >> 6;
    if (type == RDB_ENCVAL) {
        if (isencoded)
            *isencoded = 1;
        *lenptr = buf[0] & 0x3F;
    } else if (type == RDB_6BITLEN) {
        /* Read a 6 bit len. */
        *lenptr = buf[0] & 0x3F;
    } else if (type == RDB_14BITLEN) {
        if (read(mobj->source_cc->fd, buf + 1, 1) == 0)
            return -1;
        *lenptr = ((buf[0] & 0x3F) << 8) | buf[1];
    } else if (buf[0] == RDB_32BITLEN) {
        uint32_t len;
        if (read(mobj->source_cc->fd, &len, 4) == 0)
            return -1;
        *lenptr = ntohl(len);
    } else if (buf[0] == RDB_64BITLEN) {
        uint64_t len;
        if (read(mobj->source_cc->fd, &len, 8) == 0)
            return -1;
        *lenptr = ntohu64(len);
    } else {
        return -1;
    }
    return 0;
}

time_t rm_rdbLoadTime(migrateObj *mobj) {
    int32_t t32;
    if (read(mobj->source_cc->fd, &t32, 4) == 0) {
        return -1;
    }
    t32 = (char *)t32 + 4;
    return (time_t)t32;
}

long long rm_rdbLoadMillisecondTime(migrateObj *mobj, int rdbver) {
    int64_t t64;
    if (read(mobj->source_cc->fd, &t64, 8) == 0) {
        return LLONG_MAX;
    }
    if (rdbver >= 9)
        memrev64(&t64);
    return (long long)t64;
}

uint64_t rm_rdbLoadLen(migrateObj *mobj, int *isencoded) {
}

void rm_memrev64(void *p) {
    unsigned char *x = p, t;

    t = x[0];
    x[0] = x[7];
    x[7] = t;
    t = x[1];
    x[1] = x[6];
    x[6] = t;
    t = x[2];
    x[2] = x[5];
    x[5] = t;
    t = x[3];
    x[3] = x[4];
    x[4] = t;
}
