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
    assert(copy);
    snprintf(copy, len, "%s", str);

    return copy;
}

int ends_with(const char *str, const char *suffix) {
    if (!str || !suffix) return 0;

    size_t len = strlen(str);
    size_t slen = strlen(suffix);

    // A string always ends with the empty string
    if (slen == 0) {
        return 1;
    }
    else if (slen > len) {
        return 0;
    }
    else {
        return strncmp(str + (len - slen), suffix, len) == 0;
    }
}
