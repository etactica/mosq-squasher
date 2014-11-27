/* 
 * UglyLogging.  Slow, yet another wheel reinvented, but enough to make the 
 * rest of our code pretty enough.
 * 
 * Karl Palsson <karlp@remake.is>, ReMake Electric ehf. 2011
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "uglylogging.h"

static int max_level;
static bool use_syslog;

int ugly_init_syslog(int maximum_threshold) {
    max_level = maximum_threshold;
    use_syslog = true;
    return 0;
}

int ugly_init(int maximum_threshold) {
    max_level = maximum_threshold;
    if (getppid() == 1) {
        use_syslog = true;
    } else {
        use_syslog = false;
    }
    return 0;
}

int ugly_init_named(int maximum_threshold, const char *name) {
    ugly_init(maximum_threshold);
    if (use_syslog) {
        openlog(name, LOG_PID, LOG_USER);
    }
    return 0;
}

static int ugly_log_stderr(int level, const char *tag, const char *format, va_list args);
static int ugly_log_syslog(int level, const char *tag, const char *format, va_list args);

int ugly_log(int level, const char *tag, const char *format, ...) {
    if (level > max_level) {
        return 0;
    }
    va_list args;
    va_start(args, format);
    if (use_syslog) {
        ugly_log_syslog(level, tag, format, args);
    } else {
        ugly_log_stderr(level, tag, format, args);
    }
    return 0;
}

static int ugly_log_syslog(int level, const char *tag, const char *format, va_list args) {
    char tagged_format[200];
    snprintf(tagged_format, sizeof (tagged_format), "%s:%s", tag, format);
    switch (level) {
    case UDEBUG:
        vsyslog(LOG_DEBUG, tagged_format, args);
        break;
    case UINFO:
        vsyslog(LOG_INFO, tagged_format, args);
        break;
    case UWARN:
        vsyslog(LOG_WARNING, tagged_format, args);
        break;
    case UERROR:
        vsyslog(LOG_ERR, tagged_format, args);
        break;
    case UFATAL:
        vsyslog(LOG_EMERG, tagged_format, args);
        exit(EXIT_FAILURE);
        // NEVER GETS HERE!!!
        break;
    default:
        vsyslog(LOG_NOTICE, tagged_format, args);
        break;
    }
    va_end(args);
    return 1;
}

static int ugly_log_stderr(int level, const char *tag, const char *format, va_list args) {
    time_t mytt = time(NULL);
    struct tm *tt;
    tt = localtime(&mytt);
    fprintf(stderr, "%d-%02d-%02dT%02d:%02d:%02d ", tt->tm_year + 1900, tt->tm_mon + 1, tt->tm_mday, tt->tm_hour, tt->tm_min, tt->tm_sec);
    switch (level) {
        case UDEBUG:
            fprintf(stderr, "DEBUG %s: ", tag);
            break;
        case UINFO:
            fprintf(stderr, "INFO %s: ", tag);
            break;
        case UWARN:
            fprintf(stderr, "WARN %s: ", tag);
            break;
        case UERROR:
            fprintf(stderr, "ERROR %s: ", tag);
            break;
        case UFATAL:
            fprintf(stderr, "FATAL %s: ", tag);
            vfprintf(stderr, format, args); 
            exit(EXIT_FAILURE);
            // NEVER GETS HERE!!!
            break;
        default:
            fprintf(stderr, "%d %s: ", level, tag);
            break;
    }
    vfprintf(stderr, format, args); 
    va_end(args);
    return 1;
}