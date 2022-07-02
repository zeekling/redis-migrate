#include "rdbLoad.h"
#include "sds.h"

#include <errno.h>
#include <unistd.h>

int rdbLoadRioWithLoading(migrateObj *mobj) {
    char buf[1024];
    int error;
    if (syncReadLine(mobj->source_cc->fd, buf, 9, mobj->timeout) == -1) {
        serverLog(LL_WARNING, "[rm]read version failed:%s,port:%d ", mobj->host,
                  mobj->port);
        return C_ERR;
    }
    buf[9] = '\0';
    if (memcmp(buf, "REDIS", 5) != 0) {
        serverLog(LL_WARNING, "[rm] Wrong signature trying to load DB from file");
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
    long long lru_idle = -1, lfu_freq = -1, expiretime = -1, now = mstime();
    while (1) {
        sds key;
        robj *val;
        if ((type = rmLoadType(mobj)) == -1) {
            serverLog(LL_WARNING, "read type failed");
            return C_ERR;
        }

        if (type == RDB_OPCODE_EXPIRETIME) {
            expiretime = rmLoadTime(mobj);
            if (expiretime == -1) {
                return C_ERR;
            }
            expiretime *= 1000;
            continue;
        } else if (type == RDB_OPCODE_EXPIRETIME_MS) {
            expiretime = rmLoadMillisecondTime(mobj, rdbver);
            continue;
        } else if (type == RDB_OPCODE_FREQ) {
            uint8_t byte;
            if (read(mobj->source_cc->fd, &byte, 1) == 0)
                return C_ERR;
            lfu_freq = byte;
            continue;
        } else if (type == RDB_OPCODE_IDLE) {
            uint64_t qword;
            if ((qword = rmLoadLen(mobj, NULL)) == RDB_LENERR)
                return C_ERR;
            lru_idle = qword;
            continue;
        } else if (type == RDB_OPCODE_EOF) {
            break;
        } else if (type == RDB_OPCODE_SELECTDB) {
            if ((dbid = rmLoadLen(mobj, NULL)) == RDB_LENERR) return C_ERR;
            continue;
        } else if (type == RDB_OPCODE_RESIZEDB) {
            uint64_t db_size, expires_size;
            if ((db_size = rmLoadLen(mobj, NULL)) == RDB_LENERR) return C_ERR;
            if ((expires_size = rmLoadLen(mobj, NULL)) == RDB_LENERR) return C_ERR;

            continue;
        } else if (type == RDB_OPCODE_AUX) {
            robj *auxkey, *auxval;
            if ((auxkey = rmLoadStringObject(mobj)) == NULL) {
                return NULL;
            }
            if ((auxval = rmLoadStringObject(mobj)) == NULL) {
                decrRefCount(auxkey);
                return NULL;
            }
            if (((char *)auxkey->ptr)[0] == '%') {
                serverLog(LL_NOTICE, "RDB '%s': %s", (char *)auxkey->ptr, (char *)auxval->ptr);
            } else if (!strcasecmp(auxkey->ptr, "repl-stream-db")) {
                serverLog(LL_NOTICE, "repl-stream-db: %s", auxval->ptr);
            } else if (!strcasecmp(auxkey->ptr, "repl-id")) {
                memcmp(mobj->psync_replid, auxval->ptr, CONFIG_RUN_ID_SIZE + 1);
                serverLog(LL_NOTICE, "repl-id: %s", auxval->ptr);
            } else if (!strcasecmp(auxkey->ptr, "repl-offset")) {
                serverLog(LL_NOTICE, "repl-offset: %s", auxval->ptr);
            } else if (!strcasecmp(auxkey->ptr, "lua") || !strcasecmp(auxkey->ptr, "aof-base")
                       || !strcasecmp(auxkey->ptr, "redis-bits") || !strcasecmp(auxkey->ptr, "aof-preamble")) {
                // do nothing
            } else if (!strcasecmp(auxkey->ptr, "redis-ver")) {
                serverLog(LL_NOTICE, "redis-ver:%s", auxval->ptr);
            } else if (!strcasecmp(auxkey->ptr, "ctime")) {
                time_t age = time(NULL) - strtol(auxval->ptr, NULL, 10);
                if (age < 0) age = 0;
                serverLog(LL_NOTICE, "RDB age %ld seconds", (unsigned long)age);
            } else if (!strcasecmp(auxkey->ptr, "used-mem")) {
                long long usedmem = strtoll(auxval->ptr, NULL, 10);
                serverLog(LL_NOTICE, "RDB memory usage when created %.2f Mb", (double)usedmem / (1024 * 1024));
            } else {
                serverLog(LL_DEBUG, "Unrecognized RDB AUX field: '%s'", (char *)auxkey->ptr);
            }
            continue;

        } else if (type == RDB_OPCODE_FUNCTION || type == RDB_OPCODE_FUNCTION2) {
            continue;
        }
        if ((key = rmGenericLoadStringObject(mobj, RDB_LOAD_SDS, NULL)) == NULL) {
            return NULL;
        }
    }
}

robj *rmLoadStringObject(migrateObj *mobj) {
    return rmGenericLoadStringObject(mobj, RDB_LOAD_NONE, NULL);
}

void *rmGenericLoadStringObject(migrateObj *mobj, int flags, size_t *lenptr) {
    int encode = flags & RDB_LOAD_ENC;
    int plain = flags & RDB_LOAD_PLAIN;
    int sds = flags & RDB_LOAD_SDS;
    int isencoded;
    unsigned long long len;
    len = rmLoadLen(mobj, &isencoded);
    if (len == RDB_LENERR) return NULL;
    if (isencoded) {
        switch (len) {
        case RDB_ENC_INT8:
        case RDB_ENC_INT16:
        case RDB_ENC_INT32:
            return rmLoadIntegerObject(mobj, len, flags, lenptr);
        case RDB_ENC_LZF:
            return rmLoadLzfStringObject(mobj, flags, lenptr);
        default:
            serverLog(LL_WARNING, "Unknown RDB string encoding type %llu", len);
            return NULL;
        }
    }
}

void *rmLoadIntegerObject(migrateObj *mobj, int enctype, int flags, size_t *lenptr) {
    int plain = flags & RDB_LOAD_PLAIN;
    int sds = flags & RDB_LOAD_SDS;
    int encode = flags & RDB_LOAD_ENC;
    unsigned char enc[4];
    long long val;
    if (enctype == RDB_ENC_INT8) {
        if (read(mobj->source_cc->fd, enc, 1) == 0) return NULL;
        val = (signed char)enc[0];
    } else if (enctype == RDB_ENC_INT16) {
        uint16_t v;
        if (read(mobj->source_cc->fd, enc, 2) == 0) return NULL;
        v = ((uint32_t)enc[0]) | ((uint32_t)enc[1] << 8);
        val = (int16_t)v;
    } else if (enctype == RDB_ENC_INT32) {
        uint32_t v;
        if (read(mobj->source_cc->fd, enc, 4) == 0) return NULL;
        v = ((uint32_t)enc[0]) | ((uint32_t)enc[1] << 8) | ((uint32_t)enc[2] << 16) | ((uint32_t)enc[3] << 24);
        val = (int32_t)v;
    } else {
        // rdbReportCorruptRDB("Unknown RDB integer encoding type %d", enctype);
        return NULL; /* Never reached. */
    }
    if (plain || sds) {
        char buf[LONG_STR_SIZE], *p;
        int len = ll2string(buf, sizeof(buf), val);
        if (lenptr) *lenptr = len;
        p = plain ? zmalloc(len) : sdsnewlen(SDS_NOINIT, len);
        memcpy(p, buf, len);
        return p;
    } else if (encode) {
        return createStringObjectFromLongLongForValue(val);
    } else {
        return createObject(OBJ_STRING, sdsfromlonglong(val));
    }
}

void *rmLoadLzfStringObject(migrateObj *mobj, int flags, size_t *lenptr) {
    int plain = flags & RDB_LOAD_PLAIN;
    int sds = flags & RDB_LOAD_SDS;
    uint64_t len, clen;
    unsigned char *c = NULL;
    char *val = NULL;
    if ((clen = rmLoadLen(mobj, NULL)) == RDB_LENERR) return NULL;
    if ((len = rmLoadLen(mobj, NULL)) == RDB_LENERR) return NULL;
    if ((c = hi_malloc(clen)) == NULL) {
        serverLog(LL_WARNING, "rdbLoadLzfStringObject failed allocating %llu bytes", (unsigned long long)clen);
        goto err;
    }
    if (plain) {
        val = hi_malloc(len);
    } else {
        val = hi_sdsnewlen(SDS_NOINIT, len);
    }
    if (!val) {
        serverLog(LL_WARNING, "rdbLoadLzfStringObject failed allocating %llu bytes", (unsigned long long)len);
        goto err;
    }
    if (lenptr) *lenptr = len;
    if (read(mobj->source_cc->fd, c, len) == 0) return NULL;
    if (lzf_decompress(c, clen, val, len) != len) {
        serverLog(LL_WARNING, "Invalid LZF compressed string");
        goto err;
    }
    zfree(c);

    if (plain || sds) {
        return val;
    } else {
        return createObject(OBJ_STRING, val);
    }
err:
    zfree(c);
    if (plain)
        zfree(val);
    else
        sdsfree(val);
    return NULL;
}

uint64_t rmLoadLen(migrateObj *mobj, int *isencoded) {
    uint64_t len;

    if (rmLoadLenByRef(mobj, isencoded, &len) == -1)
        return RDB_LENERR;
    return len;
}

int rmLoadType(migrateObj *mobj) {
    unsigned char type;
    if (read(mobj->source_cc->fd, &type, 1) == 0) {
        return -1;
    }

    return type;
}

int rmLoadLenByRef(migrateObj *mobj, int *isencoded, uint64_t *lenptr) {
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

time_t rmLoadTime(migrateObj *mobj) {
    int32_t t32;
    if (read(mobj->source_cc->fd, &t32, 4) == 0) {
        return -1;
    }
    t32 = (char *)t32 + 4;
    return (time_t)t32;
}

long long rmLoadMillisecondTime(migrateObj *mobj, int rdbver) {
    int64_t t64;
    if (read(mobj->source_cc->fd, &t64, 8) == 0) {
        return LLONG_MAX;
    }
    if (rdbver >= 9)
        memrev64(&t64);
    return (long long)t64;
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
