
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

TARGET_NAME=redis-migrate.so
WARN=-Wall -W -Wno-missing-field-initializers
NODEPS:=clean
# Compile flags for linux / osx
ifeq ($(uname_S),Linux)
	SHOBJ_CFLAGS ?= -W -Wall -fno-common -fPIC $(DEBUG) -std=gnu99 -O2 $(WARN) $(OPTIMIZATION)
	SHOBJ_LDFLAGS ?= -shared
else
	SHOBJ_CFLAGS ?= -W -Wall -dynamic -fno-common -fPIC $(DEBUG) -std=gnu99 -O2 $(WARN) $(OPTIMIZATION)
	SHOBJ_LDFLAGS ?= -bundle -undefined dynamic_lookup
endif

FINAL_CFLAGS+= $(SHOBJ_CFLAGS)

.PHONY: all

.SUFFIXES: .c .so .xo .o

all: $(TARGET_NAME)
	@echo ""
	@echo "Hint: build redis-migrate success ;)"
	@echo ""

.c.xo:
	$(CC) -I. $(CFLAGS) $(FINAL_CFLAGS) -fPIC -c $< -o $@

sds.xo: fmacros.h sds.h sdsalloc.h
ssl.xo: hiredis.h async.h
async.xo: fmacros.h alloc.h net.h sds.h async.h
net.xo: fmacros.h net.h sds.h
ae.xo: ae.h hiredis.h async.h
read.xo: fmacros.h alloc.h sds.h
alloc.xo: fmacros.h alloc.h
dict.xo: fmacros.h alloc.h dict.h
ae.xo: redismodule.h
hiredis.xo:fmacros.h hiredis.h net.h sds.h alloc.h async.h
syncio.xo:redis-migrate.c
log.xo: log.h
rdbLoad.xo: rdbLoad.h
redis-migrate.xo: redismodule.h

XO_LIBS=redis-migrate.xo hiredis.xo sds.xo ssl.xo read.xo alloc.xo dict.xo async.xo net.xo ae.xo syncio.xo log.xo rdbLoad.xo

redis-migrate.so: $(XO_LIBS)
	$(LD) -o $@ $^ $(SHOBJ_LDFLAGS) $(LIBS) -lc

clean:
	rm -rf *.xo *.so *.o
