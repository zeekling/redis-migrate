
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

WARN=-Wall -W -Wno-missing-field-initializers
# Compile flags for linux / osx
ifeq ($(uname_S),Linux)
	SHOBJ_CFLAGS ?= -W -Wall -fno-common -g -ggdb -std=c11 -O2 $(WARN) $(OPTIMIZATION)
	SHOBJ_LDFLAGS ?= -shared
else
	SHOBJ_CFLAGS ?= -W -Wall -dynamic -fno-common -g -ggdb -std=c11 -O2 $(WARN) $(OPTIMIZATION)
	SHOBJ_LDFLAGS ?= -bundle -undefined dynamic_lookup
endif

.SUFFIXES: .c .so .xo .o


.c.xo:
	$(CC) -I. $(CFLAGS) $(SHOBJ_CFLAGS) -fPIC -c $< -o $@

all: redis-migrate.so


redis-migrate.xo: ./redismodule.h

redis-migrate.so: redis-migrate.xo
	$(LD) -o $@ $^ $(SHOBJ_LDFLAGS) $(LIBS) -lc

clean:
	rm -rf *.xo *.so *.o
