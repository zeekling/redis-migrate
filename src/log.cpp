#include <fstream>
#include <iostream>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include "log.h"
#include "redis_migrate.h"
#include "hiredis/hiredis.h"

static int is_leap_year(time_t year) {
    if (year % 4)
        return 0; /* A year not divisible by 4 is not leap. */
    else if (year % 100)
        return 1; /* If div by 4 and not 100 is surely leap. */
    else if (year % 400)
        return 0; /* If div by 100 *and* not by 400 is not leap. */
    else
        return 1; /* If div by 100 and 400 is leap. */
}

void _migrateLog(logObj mLog, int level, const char *fmt, ...) {
    va_list ap;
    char msg[LOG_MAX_LEN];

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    migrateLogRaw(mLog, level, msg);
}
void migrateLogRaw(logObj mLog, int level, const char *msg) {
    const char *c = ".-*#";

    if (level < mLog.loglevel) {
        return;
    }
    char buf[64];
    std::ofstream outfile(mLog.logfile, std::ios::app);

    int off;
    struct timeval tv;
    pid_t pid = getpid();

    gettimeofday(&tv, NULL);
    struct tm tm;
    nolocks_localtime(&tm, tv.tv_sec, timezone, 1);
    off = strftime(buf, sizeof(buf), "%d %b %Y %H:%M:%S.", &tm);
    snprintf(buf + off, sizeof(buf) - off, "%03d", (int)tv.tv_usec / 1000);
    outfile << pid << ":" << "m" << " " << buf << " " << c[level] << " <" << MODULE_NAME << "> " << msg << std::endl;
    outfile.close();
}

void nolocks_localtime(struct tm *tmp, time_t t, time_t tz, int dst) {
    const time_t secs_min = 60;
    const time_t secs_hour = 3600;
    const time_t secs_day = 3600 * 24;

    t -= tz;                       /* Adjust for timezone. */
    t += 3600 * dst;               /* Adjust for daylight time. */
    time_t days = t / secs_day;    /* Days passed since epoch. */
    time_t seconds = t % secs_day; /* Remaining seconds. */

    tmp->tm_isdst = dst;
    tmp->tm_hour = seconds / secs_hour;
    tmp->tm_min = (seconds % secs_hour) / secs_min;
    tmp->tm_sec = (seconds % secs_hour) % secs_min;

    /* 1/1/1970 was a Thursday, that is, day 4 from the POV of the tm structure
     * where sunday = 0, so to calculate the day of the week we have to add 4
     * and take the modulo by 7. */
    tmp->tm_wday = (days + 4) % 7;

    /* Calculate the current year. */
    tmp->tm_year = 1970;
    while (1) {
        /* Leap years have one day more. */
        time_t days_this_year = 365 + is_leap_year(tmp->tm_year);
        if (days_this_year > days) break;
        days -= days_this_year;
        tmp->tm_year++;
    }
    tmp->tm_yday = days; /* Number of day of the current year. */

    /* We need to calculate in which month and day of the month we are. To do
     * so we need to skip days according to how many days there are in each
     * month, and adjust for the leap year that has one more day in February. */
    int mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    mdays[1] += is_leap_year(tmp->tm_year);

    tmp->tm_mon = 0;
    while (days >= mdays[tmp->tm_mon]) {
        days -= mdays[tmp->tm_mon];
        tmp->tm_mon++;
    }

    tmp->tm_mday = days + 1; /* Add 1 since our 'days' is zero-based. */
    tmp->tm_year -= 1900;    /* Surprisingly tm_year is year-1900. */
}
