
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include "redis-migrate.h"
#include "hiredis.h"
#include "rdbLoad.h"

static migrateObj *mobj;

migrateObj *createMigrateObject(robj *host, int port, int begin_slot, int end_slot) {
    migrateObj *m;
    m = hi_malloc(sizeof(*m));
    m->host = host->ptr;
    m->port = port;
    m->begin_slot = begin_slot;
    m->end_slot = end_slot;
    m->repl_stat = REPL_STATE_NONE;
    m->isCache = 0;
    m->timeout = 10 * 1000;
    m->repl_transfer_size = -1;
    return m;
}

void freeMigrateObj(migrateObj *m) {
    redisFree(m->source_cc);
    hi_free(m);
    mobj = NULL;
}

long long ustime(void) {
    struct timeval tv;
    long long ust;

    gettimeofday(&tv, NULL);
    ust = ((long long)tv.tv_sec) * 1000000;
    ust += tv.tv_usec;
    return ust;
}

/* Return the UNIX time in milliseconds */
mstime_t mstime(void) {
    return ustime() / 1000;
}

int sendSyncCommand() {
    if (!mobj->isCache) {
        mobj->isCache = 1;
        mobj->psync_replid = "?";
        memcpy(mobj->psync_offset, "-1", 3);
    }
    if (redisAppendCommand(mobj->source_cc, "PSYNC %s %s", mobj->psync_replid, mobj->psync_offset) != REDIS_OK) {
        serverLog(LL_WARNING, "append PSYNC %s %s failed ip:%s,port:%d, ",
                  mobj->psync_replid, mobj->psync_offset, mobj->host, mobj->port);
        return 0;
    }
    if (redisFlush(mobj->source_cc) != REDIS_OK) {
        serverLog(LL_WARNING, "send PSYNC %s %s failed ip:%s,port:%d, ",
                  mobj->psync_replid, mobj->psync_offset, mobj->host, mobj->port);
        return 0;
    }
    serverLog(LL_NOTICE, "PSYNC %s %s ", mobj->psync_replid, mobj->psync_offset);
    return 1;
}

sds redisReceive() {
    char buf[256];
    if (syncReadLine(mobj->source_cc->fd, buf, sizeof(buf), mobj->timeout) == -1) {
        serverLog(LL_WARNING, "redisReceive failed ip:%s,port:%d ", mobj->host, mobj->port);
        return NULL;
    }
    return hi_sdsnew(buf);
}

int receiveDataFromRedis() {
    sds reply = redisReceive();
    if (reply == NULL) {
        serverLog(LL_WARNING, "Master did not reply to PSYNC");
        return PSYNC_TRY_LATER;
    }
    if (sdslen(reply) == 0) {
        sdsfree(reply);
        return PSYNC_WAIT_REPLY;
    }
    serverLog(LL_NOTICE, "reply=%s", reply);
    if (!strncmp(reply, "+FULLRESYNC", 11)) {
        char *replid = NULL, *offset = NULL;

        /* FULL RESYNC, parse the reply in order to extract the replid
         * and the replication offset. */
        replid = strchr(reply, ' ');
        if (replid) {
            replid++;
            offset = strchr(replid, ' ');
            if (offset)
                offset++;
        }
        if (!replid || !offset || (offset - replid - 1) != CONFIG_RUN_ID_SIZE) {
            serverLog(LL_WARNING, "Master replied with wrong +FULLRESYNC syntax.");
            /* This is an unexpected condition, actually the +FULLRESYNC
             * reply means that the master supports PSYNC, but the reply
             * format seems wrong. To stay safe we blank the master
             * replid to make sure next PSYNCs will fail. */
            memset(mobj->master_replid, 0, CONFIG_RUN_ID_SIZE + 1);
        } else {
            memcpy(mobj->master_replid, replid, offset - replid - 1);
            mobj->master_replid[CONFIG_RUN_ID_SIZE] = '\0';
            mobj->master_initial_offset = strtoll(offset, NULL, 10);
            serverLog(LL_NOTICE, "Full sync from master: %s:%lld",
                      mobj->master_replid, mobj->master_initial_offset);
        }
        sdsfree(reply);
        return PSYNC_FULLRESYNC;
    }
    if (!strncmp(reply, "+CONTINUE", 9)) {
        /* Partial resync was accepted. */
        serverLog(LL_NOTICE, "Successful partial resynchronization with master.");

        /* Check the new replication ID advertised by the master. If it
         * changed, we need to set the new ID as primary ID, and set
         * secondary ID as the old master ID up to the current offset, so
         * that our sub-slaves will be able to PSYNC with us after a
         * disconnection. */
        char *start = reply + 10;
        char *end = reply + 9;
        while (end[0] != '\r' && end[0] != '\n' && end[0] != '\0')
            end++;
        if (end - start == CONFIG_RUN_ID_SIZE) {
            char new[CONFIG_RUN_ID_SIZE + 1];
            memcpy(new, start, CONFIG_RUN_ID_SIZE);
            new[CONFIG_RUN_ID_SIZE] = '\0';

            return PSYNC_CONTINUE;
        }
    }
}

void readFullData() {
    static char eofmark[CONFIG_RUN_ID_SIZE];
    static char lastbytes[CONFIG_RUN_ID_SIZE];
    static int usemark = 0;
    char buf[PROTO_IOBUF_LEN];
    if (mobj->repl_transfer_size == -1) {
        int nread = syncReadLine(mobj->source_cc->fd, buf, PROTO_IOBUF_LEN, mobj->timeout);
        if (nread == -1) {
            serverLog(LL_WARNING, "read full data failed");
            goto error;
        }
        if (buf[0] == '-') {
            serverLog(LL_WARNING, "MASTER aborted replication with an error: %s", buf + 1);
            goto error;
        } else if (buf[0] == '\0') {
            // mobj->repl_transfer_lastio = server.unixtime;
            return;
        } else if (buf[0] != '$') {
            serverLog(LL_WARNING, "Bad protocol from MASTER, the first byte is not '$' (we received '%s'), are you sure the host and port are right?", buf);
            goto error;
        }

        if (strncmp(buf + 1, "EOF:", 4) == 0 && strlen(buf + 5) >= CONFIG_RUN_ID_SIZE) {
            usemark = 1;
            usemark = 1;
            memcpy(eofmark, buf + 5, CONFIG_RUN_ID_SIZE);
            memset(lastbytes, 0, CONFIG_RUN_ID_SIZE);
            /* Set any repl_transfer_size to avoid entering this code path
             * at the next call. */
            mobj->repl_transfer_size = 0;
            serverLog(LL_NOTICE, "MASTER <-> REPLICA sync: receiving streamed RDB from master with EOF to parser");
        } else {
            usemark = 0;
            mobj->repl_transfer_size = strtol(buf + 1, NULL, 10);
            serverLog(LL_NOTICE,
                      "MASTER <-> REPLICA sync: receiving %lld bytes from master to parser",
                      (long long)mobj->repl_transfer_size);
        }
    }
    int flag = rmLoadRioWithLoading(mobj);

    return;
error:
    cancelMigrate();
    return;
}

void cancelMigrate() {
}

void syncDataWithRedis(int fd, void *user_data, int mask) {
    REDISMODULE_NOT_USED(fd);
    REDISMODULE_NOT_USED(mask);
    REDISMODULE_NOT_USED(user_data);
    sds err = NULL;
    if (mobj->repl_stat == REPL_STATE_CONNECTING) {
        if (redisSendCommand(mobj->source_cc, "PING") != REDIS_OK) {
            serverLog(LL_WARNING, "send PING failed ip:%s,port:%d",
                      mobj->host, mobj->port);
            goto error;
        }
        mobj->repl_stat = REPL_STATE_RECEIVE_PING_REPLY;
        return;
    }
    if (mobj->repl_stat == REPL_STATE_RECEIVE_PING_REPLY) {
        err = redisReceive();
        if (err == NULL)
            goto no_response_error;
        if (err[0] != '+' && strncmp(err, "-NOAUTH", 7) != 0 && strncmp(err, "-NOPERM", 7) != 0 && strncmp(err, "-ERR operation not permitted", 28) != 0) {
            serverLog(LL_WARNING, "Error reply to PING from master: '%s'", err);
            sdsfree(err);
            goto error;
        } else {
            serverLog(LL_NOTICE, "Master replied to PING, replication can continue...");
        }
        sdsfree(err);
        err = NULL;
        mobj->repl_stat = REPL_STATE_SEND_HANDSHAKE;
        return;
    }
    if (mobj->repl_stat == REPL_STATE_SEND_HANDSHAKE) {
        // todo 增加认证
        mobj->repl_stat = REPL_STATE_RECEIVE_AUTH_REPLY;
    }
    if (mobj->repl_stat == REPL_STATE_RECEIVE_AUTH_REPLY) {
        // todo 接受认证信息
        sds portstr = sdsfromlonglong(mobj->port);
        if (redisSendCommand(mobj->source_cc, "REPLCONF listening-port %s", portstr) != REDIS_OK) {
            serverLog(LL_WARNING, "send PING failed ip:%s,port:%d",
                      mobj->host, mobj->port);
            goto error;
        }
        mobj->repl_stat = REPL_STATE_RECEIVE_PORT_REPLY;
        sdsfree(portstr);
        return;
    }
    if (mobj->repl_stat == REPL_STATE_RECEIVE_PORT_REPLY) {
        err = redisReceive();
        if (err == NULL)
            goto no_response_error;
        if (err[0] == '-') {
            serverLog(LL_NOTICE, "(Non critical) Master does not understand REPLCONF listening-port: %s", err);
            goto error;
        }
        serverLog(LL_NOTICE, "REPLCONF listening-port success");
        if (redisSendCommand(mobj->source_cc, "REPLCONF ip-address %s", mobj->host) != REDIS_OK) {
            serverLog(LL_WARNING, "REPLCONF ip-address %s failed", mobj->host);
            goto error;
        }
        sdsfree(err);
        err = NULL;
        mobj->repl_stat = REPL_STATE_RECEIVE_IP_REPLY;
        return;
    }
    if (mobj->repl_stat == REPL_STATE_RECEIVE_IP_REPLY) {
        err = redisReceive();
        if (err == NULL)
            goto no_response_error;
        if (err[0] == '-') {
            serverLog(LL_NOTICE, "(Non critical) Master does not understand REPLCONF ip-address: %s", err);
            goto error;
        }
        serverLog(LL_NOTICE, "REPLCONF REPLCONF ip-address success");
        if (redisSendCommand(mobj->source_cc, "REPLCONF %s %s %s %s", "capa", "eof", "capa", "psync2") != REDIS_OK) {
            serverLog(LL_WARNING, "send REPLCONF capa eof capa psync2 failed");
            goto error;
        }
        sdsfree(err);
        err = NULL;
        mobj->repl_stat = REPL_STATE_RECEIVE_CAPA_REPLY;
        return;
    }
    if (mobj->repl_stat == REPL_STATE_RECEIVE_CAPA_REPLY) {
        err = redisReceive();
        if (err == NULL)
            goto no_response_error;
        if (err[0] == '-') {
            serverLog(LL_NOTICE, "(Non critical) Master does not understand REPLCONF capa: %s", err);
            goto error;
        }
        serverLog(LL_NOTICE, "REPLCONF capa eof capa psync2 success");
        sdsfree(err);
        err = NULL;
        mobj->repl_stat = REPL_STATE_SEND_PSYNC;
        return;
    }
    if (mobj->repl_stat == REPL_STATE_SEND_PSYNC) {
        if (!sendSyncCommand()) {
            serverLog(LL_WARNING, "send PSYNC %s %s failed ip:%s,port:%d, ",
                      mobj->psync_replid, mobj->psync_offset, mobj->host, mobj->port);
            goto error;
        }
        mobj->repl_stat = REPL_STATE_RECEIVE_PSYNC_REPLY;
        return;
    }

    if (mobj->repl_stat == REPL_STATE_RECEIVE_PSYNC_REPLY) {
        int psync_result = receiveDataFromRedis();
        if (psync_result == PSYNC_WAIT_REPLY)
            return;
        if (psync_result == PSYNC_TRY_LATER)
            goto error;
        if (psync_result == PSYNC_CONTINUE) {
            mobj->repl_stat = REPL_STATE_CONTINUE_SYNC;
        } else {
            mobj->repl_stat = REPL_STATE_FULL_SYNC;
        }
    }

    // 接受全部数据
    if (mobj->repl_stat == REPL_STATE_FULL_SYNC) {
        serverLog(LL_NOTICE, "begin receive full data");
        mobj->repl_stat = REPL_STATE_READING_FULL_DATA;
        readFullData();
    }
    if (mobj->repl_stat == REPL_STATE_CONTINUE_SYNC) {
        serverLog(LL_NOTICE, "begin receive continue data");
    }

    return;
error:
    if (err != NULL) {
        sdsfree(err);
    }
    freeMigrateObj(mobj);
no_response_error: /* Handle receiveSynchronousResponse() error when master has no reply */
    serverLog(LL_WARNING, "Master did not respond to command during SYNC handshake");
}

/**
 * migrate data to current instance.
 * migrate host port begin-slot end-slot
 * @param ctxz
 * @param argv
 * @param argc
 * @return
 */
int rm_migrateCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 5) {
        return RedisModule_WrongArity(ctx);
    }
    if (RedisModule_IsKeysPositionRequest(ctx)) {
        RedisModule_Log(ctx, VERBOSE, "get keys from module");
        return RedisModule_ReplyWithSimpleString(ctx, "OK");
    }
    robj *host = (robj *)argv[1];
    robj *port = (robj *)argv[2];
    robj *begin_slot = (robj *)argv[3];
    robj *end_slot = (robj *)argv[4];
    RedisModule_Log(ctx, NOTICE, "host:%s, port:%s, begin:%s, end:%s", (char *)host->ptr,
                    (char *)port->ptr, (char *)begin_slot->ptr, (char *)end_slot->ptr);
    if (mobj != NULL) {
        return RedisModule_ReplyWithError(ctx, "migrating, please waiting");
    }
    mobj = createMigrateObject(host, atoi(port->ptr), atoi(begin_slot->ptr), atoi(end_slot->ptr));
    struct timeval timeout = {1, 500000}; // 1.5s
    redisOptions options = {0};
    REDIS_OPTIONS_SET_TCP(&options, (const char *)mobj->host, mobj->port);
    options.connect_timeout = &timeout;
    mobj->source_cc = redisConnectWithOptions(&options);
    if (mobj->source_cc == NULL || mobj->source_cc->err) {
        RedisModule_Log(ctx, WARNING, "Could not connect to Redis at ip:%s,port:%d, error:%s",
                        mobj->host, mobj->port, mobj->source_cc->errstr);
        freeMigrateObj(mobj);
        return RedisModule_ReplyWithError(ctx, "Can't connect source redis");
    }
    mobj->repl_stat = REPL_STATE_CONNECTING;
    RedisModule_EventLoopAdd(mobj->source_cc->fd, AE_WRITABLE, syncDataWithRedis, ctx);
    return RedisModule_ReplyWithSimpleString(ctx, "OK");
}

int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    REDISMODULE_NOT_USED(argv);
    REDISMODULE_NOT_USED(argc);

    int flag = RedisModule_Init(ctx, MODULE_NAME, REDIS_MIGRATE_VERSION, REDISMODULE_APIVER_1);
    if (flag == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModule_Log(ctx, NOTICE, "begin init commands of %s", MODULE_NAME);
    flag = RedisModule_CreateCommand(ctx, "rm.migrate", rm_migrateCommand, "write deny-oom admin getkeys-api", 0, 0, 0);
    if (flag == REDISMODULE_ERR) {
        RedisModule_Log(ctx, WARNING, "init rm.migrate failed");
        return REDISMODULE_ERR;
    }
    RedisModuleCallReply *reply = RedisModule_Call(ctx, "config", "cc", "get", "logfile");
    long long items = RedisModule_CallReplyLength(reply);
    if (items != 2) {
        RedisModule_Log(ctx, WARNING, "logfile is empty");
        return REDISMODULE_ERR;
    }
    RedisModuleCallReply *item1 = RedisModule_CallReplyArrayElement(reply, 1);
    robj *logfile = (robj *)RedisModule_CreateStringFromCallReply(item1);
    RedisModule_Log(ctx, NOTICE, "logfile is %s", (char *)logfile->ptr);
    createLogObj((char *)logfile->ptr);
    RedisModule_Log(ctx, NOTICE, "init %s success", MODULE_NAME);
    return REDISMODULE_OK;
}
