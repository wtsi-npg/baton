/**
 * Copyright (c) 2013-2014 Genome Research Ltd. All rights reserved.
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
 * @file utilities.c
 * @author Keith James <kdj@sanger.ac.uk>
 */

// For strptime
#include <config.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "log.h"

char *copy_str(const char *str) {
    size_t len = strlen(str) + 1;
    char *copy = calloc(len, sizeof (char));
    if (!copy) goto error;

    snprintf(copy, len, "%s", str);

    return copy;

error:
    logmsg(ERROR, "Failed to allocate memory: error %d %s",
           errno, strerror(errno));

    return NULL;
}

int str_starts_with(const char *str, const char *prefix) {
    if (!str || !prefix) return 0;

    size_t len  = strlen(str);
    size_t plen = strlen(prefix);

    // A string always starts with the empty string
    if (plen == 0)  return 1;
    if (plen > len) return 0;

    return strncmp(str, prefix, plen) == 0;
}

int str_equals(const char *str1, const char *str2) {
    return (strncmp(str1, str2, strlen(str2)) == 0);
}

int str_equals_ignore_case(const char *str1, const char *str2) {
    return (strncasecmp(str1, str2, strlen(str2)) == 0);
}

int str_ends_with(const char *str, const char *suffix) {
    if (!str || !suffix) return 0;

    size_t len  = strlen(str);
    size_t slen = strlen(suffix);

    // A string always ends with the empty string
    if (slen == 0)  return 1;
    if (slen > len) return 0;

    return strncmp(str + (len - slen), suffix, len) == 0;
}

const char *parse_base_name(const char *path) {
    const char delim = '/';

    const char *base_name = strrchr(path, delim);

    // Base name should not include the '/'
    if (base_name) {
        base_name++;
    }
    else {
        base_name = path;
    }

    return base_name;
}

FILE *maybe_stdin(const char *path) {
    FILE *stream;

    if (path) {
        stream = fopen(path, "r");
        if (!stream) goto error;
    }
    else {
        stream = stdin;
    }

    return stream;

error:
    logmsg(ERROR, "Failed to open '%s': error %d %s",
           path, errno, strerror(errno));

    return NULL;
}

char *format_timestamp(const char *raw_timestamp, const char *format) {
    int buffer_len = 32;
    char *buffer;
    int base = 10;

    struct tm tm;
    time_t time;
    int status;

    buffer = calloc(buffer_len, sizeof (char));
    if (!buffer) {
        logmsg(ERROR, "Failed to allocate memory: error %d %s",
               errno, strerror(errno));
        goto error;
    }

    errno = 0;
    time = strtoul(raw_timestamp, NULL, base);
    if (errno != 0) {
        logmsg(ERROR, "Failed to convert timestamp '%s' to a number: "
               "error %d %s", raw_timestamp, errno, strerror(errno));
        goto error;
    }

    gmtime_r(&time, &tm);

    status = strftime(buffer, buffer_len, format, &tm);
    if (status == 0) {
        logmsg(ERROR, "Failed to format timestamp '%s' as an ISO date time: "
               "error %d %s", raw_timestamp, errno, strerror(errno));
        goto error;
    }

    logmsg(DEBUG,"Converted timestamp '%s' to '%s'", raw_timestamp, buffer);

    return buffer;

error:
    if (buffer) free(buffer);

    return NULL;
}

char *parse_timestamp(const char *timestamp, const char *format) {
    int buffer_len = 32;
    char *buffer;
    char *rest;

    struct tm tm;
    time_t time;

    buffer = calloc(buffer_len, sizeof (char));
    if (!buffer) {
        logmsg(ERROR, "Failed to allocate memory: error %d %s",
               errno, strerror(errno));
        goto error;
    }

    rest = strptime(timestamp, format, &tm);
    if (!rest) {
        logmsg(ERROR, "Failed to parse ISO date time '%s'", timestamp);
        goto error;
    }

    time = timegm(&tm);
    snprintf(buffer, buffer_len, "%ld", time);

    logmsg(DEBUG, "Parsed timestamp '%s' to '%ld'", timestamp, time);

    return buffer;

error:
    if (buffer) free(buffer);

    return NULL;
}
