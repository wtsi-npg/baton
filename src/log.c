/**
 * Copyright (C) 2014 Genome Research Ltd. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @file log.c
 * @author Keith James <kdj@sanger.ac.uk>
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "log.h"
#include "utilities.h"

static log_level THRESHOLD = WARN;

const char *get_log_level_name(log_level level) {
    switch (level) {
        case FATAL:
            return "FATAL";
        case ERROR:
            return "ERROR";
        case WARN:
            return "WARN";
        case NOTICE:
            return "NOTICE";
        case INFO:
            return "INFO";
        case TRACE:
            return "TRACE";
        case DEBUG:
            return "DEBUG";
        default:
            return "UNKNOWN";
    }
}

log_level get_log_threshold() {
    return THRESHOLD;
}

log_level set_log_threshold(log_level level) {
    logmsg(DEBUG, "Setting log level to %s", get_log_level_name(level));

    THRESHOLD = level;

    return THRESHOLD;
}

void log_impl(int line, const char *file, char const *function,
              log_level level, ...) {
    if (level <= THRESHOLD) {
        char buffer[32];

        time_t t = time(0);
        struct tm tm;
        gmtime_r(&t, &tm);

        int status = strftime(buffer, sizeof buffer, ISO8601_FORMAT, &tm);
        if (status == 0) {
            fprintf(stderr, "Failed to format timestamp '%s': error %d %s",
                    buffer, errno, strerror(errno));
            goto error;
        }

        fprintf(stderr, "%s %s ", buffer, get_log_level_name(level));

        if (level >= TRACE) {
            fprintf(stderr, "%s:%d:%s: ", file, line, function);
        }

        va_list args;
        va_start(args, level);
        const char *format = va_arg(args, char *);

        vfprintf(stderr, format, args);
        va_end(args);

        fprintf(stderr, "\n");
        fflush(stderr);
    }

error:
    return;
}
