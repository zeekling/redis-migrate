#ifndef __HIREDIS_AE_H__
#define __HIREDIS_AE_H__

#include "hiredis.h"
#include "async.h"

#define AE_READABLE 1   /* Fire when descriptor is readable. */
#define AE_WRITABLE 2   /* Fire when descriptor is writable. */


int aeWait(int fd, int mask, long long milliseconds);

#endif
