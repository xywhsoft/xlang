/* ntp_native.c - NTP time utilities (no network dependency for TCC compatibility) */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/* NTP epoch offset: seconds between 1900-01-01 and 1970-01-01 */
#define NTP_EPOCH_OFFSET 2208988800UL

static char err_buf[256];
static char time_buf[64];

const char *xntp_error(void) { return err_buf; }

/* Convert NTP timestamp (seconds since 1900) to Unix timestamp */
long long xntp_to_unix(long long ntp_sec) {
    return ntp_sec - (long long)NTP_EPOCH_OFFSET;
}

/* Convert Unix timestamp to NTP timestamp */
long long xntp_from_unix(long long unix_sec) {
    return unix_sec + (long long)NTP_EPOCH_OFFSET;
}

/* Get local time as Unix timestamp */
long long xntp_local_time(void) {
    return (long long)time(NULL);
}

/* Format Unix timestamp as ISO string (UTC) */
const char *xntp_format(long long unix_ts) {
    time_t t = (time_t)unix_ts;
    struct tm *tm = gmtime(&t);
    if (!tm) return "invalid";
    sprintf(time_buf, "%04d-%02d-%02dT%02d:%02d:%02dZ",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec);
    return time_buf;
}

/* Format as local time */
const char *xntp_format_local(long long unix_ts) {
    time_t t = (time_t)unix_ts;
    struct tm *tm = localtime(&t);
    if (!tm) return "invalid";
    sprintf(time_buf, "%04d-%02d-%02d %02d:%02d:%02d",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec);
    return time_buf;
}

/*
 * Query NTP server - requires network library.
 * Returns -1 (not available in TCC build without ws2_32).
 * In a full build, this would perform UDP query to port 123.
 */
long long xntp_query(const char *server) {
    (void)server;
    strcpy(err_buf, "ntp: network query not available in this build");
    return -1;
}

/* Get timezone offset in seconds from UTC */
long long xntp_tz_offset(void) {
    time_t now = time(NULL);
    struct tm local_tm, utc_tm;
    struct tm *lt = localtime(&now);
    struct tm *ut = gmtime(&now);
    if (!lt || !ut) return 0;
    local_tm = *lt;
    utc_tm = *ut;
    long long lsec = (long long)(local_tm.tm_hour * 3600 + local_tm.tm_min * 60 + local_tm.tm_sec);
    long long usec = (long long)(utc_tm.tm_hour * 3600 + utc_tm.tm_min * 60 + utc_tm.tm_sec);
    long long diff = lsec - usec;
    /* Handle day boundary */
    if (local_tm.tm_mday > utc_tm.tm_mday) diff += 86400;
    if (local_tm.tm_mday < utc_tm.tm_mday) diff -= 86400;
    return diff;
}

/* Get day of week (0=Sunday) for a Unix timestamp */
long long xntp_day_of_week(long long unix_ts) {
    time_t t = (time_t)unix_ts;
    struct tm *tm = gmtime(&t);
    if (!tm) return -1;
    return (long long)tm->tm_wday;
}

/* Get days in month for a given year/month */
long long xntp_days_in_month(long long year, long long month) {
    static int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 0;
    if (month == 2) {
        int leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        return leap ? 29 : 28;
    }
    return days[month];
}
