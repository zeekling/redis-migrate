#ifndef __HIREDIS_AE_H__
#define __HIREDIS_AE_H__


#include "hiredis.h"
#include "async.h"


typedef uint64_t monotime;

#define AE_OK 0
#define AE_ERR -1
#define AE_NONE 0       /* No events registered. */
#define AE_READABLE 1   /* Fire when descriptor is readable. */
#define AE_WRITABLE 2   /* Fire when descriptor is writable. */
#define AE_BARRIER 4    /* With WRITABLE, never fire the event if the
                           READABLE event already fired in the same event
                           loop iteration. Useful when you want to persist
                           things to disk before sending replies, and want
                           to do that in a group fashion. */

/* Types and data structures */
typedef void aeFileProc(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask);

typedef int aeTimeProc(struct aeEventLoop *eventLoop, long long id, void *clientData);

typedef void aeEventFinalizerProc(struct aeEventLoop *eventLoop, void *clientData);

typedef void aeBeforeSleepProc(struct aeEventLoop *eventLoop);

/* File event structure */
typedef struct aeFileEvent {
    int mask; /* one of AE_(READABLE|WRITABLE|BARRIER) */
    aeFileProc *rfileProc;
    aeFileProc *wfileProc;
    void *clientData;
} aeFileEvent;

/* Time event structure */
typedef struct aeTimeEvent {
    long long id; /* time event identifier. */
    monotime when;
    aeTimeProc *timeProc;
    aeEventFinalizerProc *finalizerProc;
    void *clientData;
    struct aeTimeEvent *prev;
    struct aeTimeEvent *next;
    int refcount; /* refcount to prevent timer events from being
  		   * freed in recursive time event calls. */
} aeTimeEvent;

/* A fired event */
typedef struct aeFiredEvent {
    int fd;
    int mask;
} aeFiredEvent;

typedef struct aeEventLoop {
    int maxfd;   /* highest file descriptor currently registered */
    int setsize; /* max number of file descriptors tracked */
    long long timeEventNextId;
    aeFileEvent *events; /* Registered events */
    aeFiredEvent *fired; /* Fired events */
    aeTimeEvent *timeEventHead;
    int stop;
    void *apidata; /* This is used for polling API specific data */
    aeBeforeSleepProc *beforesleep;
    aeBeforeSleepProc *aftersleep;
    int flags;
} aeEventLoop;

typedef struct redisAeEvents {
    redisAsyncContext *context;
    aeEventLoop *loop;
    int fd;
    int reading, writing;
} redisAeEvents;

typedef struct aeApiState {
    int epfd;
    struct epoll_event *events;
} aeApiState;

#define AE_FILE_EVENTS (1<<0)
#define AE_TIME_EVENTS (1<<1)
#define AE_ALL_EVENTS (AE_FILE_EVENTS|AE_TIME_EVENTS)
#define AE_DONT_WAIT (1<<2)
#define AE_CALL_BEFORE_SLEEP (1<<3)
#define AE_CALL_AFTER_SLEEP (1<<4)

#define AE_NOMORE -1
#define AE_DELETED_EVENT_ID -1

extern monotime (*getMonotonicUs)(void);

void redisAeReadEvent(aeEventLoop *el, int fd, void *privdata, int mask);

void redisAeWriteEvent(aeEventLoop *el, int fd, void *privdata, int mask);

int aeApiAddEvent(aeEventLoop *eventLoop, int fd, int mask);

void aeStop(aeEventLoop *eventLoop);

void aeMain(aeEventLoop *eventLoop);

int aeProcessEvents(aeEventLoop *eventLoop, int flags);

int aeCreateFileEvent(aeEventLoop *eventLoop, int fd, int mask,
                      aeFileProc *proc, void *clientData);

void redisAeAddRead(void *privdata);

void aeApiDelEvent(aeEventLoop *eventLoop, int fd, int delmask);

void aeDeleteFileEvent(aeEventLoop *eventLoop, int fd, int mask);

void redisAeDelRead(void *privdata);

void redisAeAddWrite(void *privdata);

void redisAeDelWrite(void *privdata);

void redisAeCleanup(void *privdata);

int anetCloexec(int fd);

static int aeApiCreate(aeEventLoop *eventLoop);

aeEventLoop *aeCreateEventLoop(int setsize);

int redisAeAttach(aeEventLoop *loop, redisAsyncContext *ac);

#endif
