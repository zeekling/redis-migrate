
#include <sys/time.h>
#include <unistd.h>
#include <threads.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "redis-migrate.h"
#include "hiredis/hiredis.h"
#include "redismodule.h"
#include "log.h"

static logObj mLog = {};

static RedisModuleCommandFilter *filter;

static bool isMigrating = false;

migrateObj createMigrateObject(RedisModuleString *host, int port, int slot, RedisModuleString *key, migrateObj m) {
    size_t hostLen, keyLen;
    const char *hostStr = RedisModule_StringPtrLen(host, &hostLen);
    const char *keyStr = RedisModule_StringPtrLen(key, &keyLen);
    m.port = port;
    m.slot = slot;
    m.timeout = 10000;
    m.isCache = false;
    m.host = hostStr;
    m.hostLen = hostLen;
    m.key = keyStr;
    m.keyLen = keyLen;
    return m;
}

migrateObj findMigrating(std::string key) {
    std::map<std::string, migrateObj>::iterator p;
    p = migrating.find(key);
    if (p != migrating.end()) {
        return p->second;
    }
    return {};
}

static void initLogObj(RedisModuleCtx *ctx) {
    if (mLog.logfile != nullptr) {
        return;
    }
    RedisModuleCallReply *reply = RedisModule_Call(ctx, "config", "cc", "get", "logfile");
    long long items = RedisModule_CallReplyLength(reply);
    if (items != 2) {
        RedisModule_Log(ctx, WARNING, "logfile is empty");
        return;
    }
    RedisModuleCallReply *item1 = RedisModule_CallReplyArrayElement(reply, 1);
    RedisModuleString *logfileStr = RedisModule_CreateStringFromCallReply(item1);
    size_t logfileLen;
    const char *logfile = RedisModule_StringPtrLen(logfileStr, &logfileLen);
    RedisModule_Log(ctx, NOTICE, "init logfile success logfile is %s", logfile);
    mLog.logfile = logfile;
    mLog.loglevel = LL_NOTICE;
}

int rm_migrateCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 5) {
        return RedisModule_ReplyWithError(ctx, "args number failed");
    }
    if (RedisModule_IsKeysPositionRequest(ctx)) {
        RedisModule_Log(ctx, VERBOSE, "get keys from module");
        return RedisModule_ReplyWithSimpleString(ctx, "OK");
    }

    RedisModuleString *host = argv[1];
    double portDouble, slotDouble;
    if (RedisModule_StringToDouble(argv[2], &portDouble) != REDISMODULE_OK) {
        return RedisModule_ReplyWithError(ctx, "ERR invalid port");
    }
    if (RedisModule_StringToDouble(argv[3], &slotDouble) != REDISMODULE_OK) {
        return RedisModule_ReplyWithError(ctx, "ERR invalid port");
    }

    RedisModuleString *key = argv[4];
    if (migrating.size() >= MAX_MIGRATEING_NUM) {
        return RedisModule_ReplyWithError(ctx, "migrating key is bigger 50");
    }
    initLogObj(ctx);
    size_t keyLen;
    const char *keyChar = RedisModule_StringPtrLen(key, &keyLen);
    std::string keyStr(keyChar, keyLen);
    migrateObj m = findMigrating(keyStr);
    if (m.hostLen > 0) {
        return RedisModule_ReplyWithError(ctx, "migrating, try again later");
    }

    m = createMigrateObject(host, (int)portDouble, (int)slotDouble, key, m);
    migrating[keyStr] = m;
    isMigrating = true;

    return RedisModule_ReplyWithSimpleString(ctx, "OK");
}

void rm_migrateFilter(RedisModuleCommandFilterCtx *filter) {
    if (!isMigrating) {
        return;
    }

    int pos = 0;
    bool isMigratingKey = false;

    while (pos < RedisModule_CommandFilterArgsCount(filter)) {
        const RedisModuleString *arg = RedisModule_CommandFilterArgGet(filter, pos);
        size_t arg_len;
        const char *arg_str = RedisModule_StringPtrLen(arg, &arg_len);
        if (isMigratingKey) {
            RedisModule_CommandFilterArgDelete(filter, pos);
            continue;
        }

        if (pos == 1) {
            std::string keyStr(arg_str, arg_len);
            migrateObj m = findMigrating(keyStr);
            if (m.hostLen > 0) {
                isMigratingKey = true;
                migrateLog(mLog, LL_NOTICE, "key:%s is migrating, do not allow change", arg_str);
                RedisModule_CommandFilterArgDelete(filter, pos);
                continue;
            }
        }

        pos++;
    }
}

#ifdef __cplusplus
extern "C" {
#endif

int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    REDISMODULE_NOT_USED(argv);
    REDISMODULE_NOT_USED(argc);

    int flag = RedisModule_Init(ctx, MODULE_NAME, REDIS_MIGRATE_VERSION, REDISMODULE_APIVER_1);
    if (flag == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModule_Log(ctx, NOTICE, "begin init commands of %s", "rm.migrate");
    flag = RedisModule_CreateCommand(ctx, "rm.migrate", rm_migrateCommand, "write deny-oom admin getkeys-api", 0, 0, 0);
    if (flag == REDISMODULE_ERR) {
        RedisModule_Log(ctx, WARNING, "init rm.migrate failed");
        return REDISMODULE_ERR;
    }
    filter = RedisModule_RegisterCommandFilter(ctx, rm_migrateFilter, 0);
    if (filter == NULL) {
        RedisModule_Log(ctx, WARNING, "init filter failed");
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
#ifdef __cplusplus
}
#endif
