
#include "redis-migrate.h"
#include "ae.h"
#include <unistd.h>
#include <errno.h>

/* ----------------- Blocking sockets I/O with timeouts --------------------- */

#define SYNCIO__RESOLUTION 10 

ssize_t syncWrite(int fd, char *ptr, ssize_t size, long long timeout)
{
    ssize_t nwritten, ret = size;
    long long start = mstime();
    long long remaining = timeout;

    while (1)
    {
        long long wait = (remaining > SYNCIO__RESOLUTION) ? remaining : SYNCIO__RESOLUTION;
        long long elapsed;

        nwritten = write(fd, ptr, size);
        if (nwritten == -1)
        {
            if (errno != EAGAIN)
                return -1;
        }
        else
        {
            ptr += nwritten;
            size -= nwritten;
        }
        if (size == 0)
            return ret;

        /* Wait */
        aeWait(fd, AE_WRITABLE, wait);
        elapsed = mstime() - start;
        if (elapsed >= timeout)
        {
            errno = ETIMEDOUT;
            return -1;
        }
        remaining = timeout - elapsed;
    }
}

ssize_t syncRead(int fd, char *ptr, ssize_t size, long long timeout)
{
    ssize_t nread, totread = 0;
    long long start = mstime();
    long long remaining = timeout;

    if (size == 0)
        return 0;
    while (1)
    {
        long long wait = (remaining > SYNCIO__RESOLUTION) ? remaining : SYNCIO__RESOLUTION;
        long long elapsed;

        nread = read(fd, ptr, size);
        if (nread == 0)
            return -1; 
        if (nread == -1)
        {
            if (errno != EAGAIN)
                return -1;
        }
        else
        {
            ptr += nread;
            size -= nread;
            totread += nread;
        }
        if (size == 0)
            return totread;

        /* Wait */
        aeWait(fd, AE_READABLE, wait);
        elapsed = mstime() - start;
        if (elapsed >= timeout)
        {
            errno = ETIMEDOUT;
            return -1;
        }
        remaining = timeout - elapsed;
    }
}

ssize_t syncReadLine(int fd, char *ptr, ssize_t size, long long timeout)
{
    ssize_t nread = 0;

    size--;
    while (size)
    {
        char c;

        if (syncRead(fd, &c, 1, timeout) == -1)
            return -1;
        if (c == '\n')
        {
            *ptr = '\0';
            if (nread && *(ptr - 1) == '\r')
                *(ptr - 1) = '\0';
            return nread;
        }
        else
        {
            *ptr++ = c;
            *ptr = '\0';
            nread++;
        }
        size--;
    }
    return nread;
}
