/**
 * Copyright (c) 2013 Genome Research Ltd. All rights reserved.
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
 * @author Keith James <kdj@sanger.ac.uk>
 */

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlog.h>

#include <baton.h>

void logmsg(log_level level, const char* category, const char *format, ...) {
    va_list args;
    va_start(args, format);

    zlog_category_t *cat = zlog_get_category(category);
    if (!cat) {
        fprintf(stderr, "Failed to get zlog category '%s'\n", category);
        goto error;
    }

    switch (level) {
        case FATAL:
            vzlog_fatal(cat, format, args);
            break;

        case ERROR:
            vzlog_error(cat, format, args);
            break;

        case WARN:
            vzlog_warn(cat, format, args);
            break;

        case NOTICE:
            vzlog_notice(cat, format, args);
            break;

        case INFO:
            vzlog_info(cat, format, args);
            break;

        case TRACE:
            vzlog_trace(cat, format, args);
            break;

        case DEBUG:
            vzlog_debug(cat, format, args);
            break;

        default:
            vzlog_debug(cat, format, args);
    }

    va_end(args);
    return;

error:
    va_end(args);
    return;
}

char *copy_str(const char *str) {
    size_t len = strlen(str) + 1;
    char *copy = calloc(len, sizeof (char));
    if (!copy) goto error;

    snprintf(copy, len, "%s", str);

    return copy;

error:
    logmsg(ERROR, BATON_CAT, "Failed to allocate memory: error %d %s",
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
    logmsg(ERROR, BATON_CAT, "Failed to open '%s': error %d %s",
           path, errno, strerror(errno));

    return NULL;
}
