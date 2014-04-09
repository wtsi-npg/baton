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
 * @file log.c
 * @author Keith James <kdj@sanger.ac.uk>
 */

#include <zlog.h>

#include <log.h>

static char *SYSTEM_LOG_CONF_FILE = ZLOG_CONF; // Set by autoconf

int start_logging(const char *log_file) {
    int status;

    if (!log_file) {
        status = zlog_init(SYSTEM_LOG_CONF_FILE);

        if (status != 0) {
            fprintf(stderr, "Logging configuration failed "
                    "(using system-defined configuration in '%s')\n",
                    SYSTEM_LOG_CONF_FILE);
        }
    }
    else {
        status = zlog_init(log_file);

        if (status != 0) {
            fprintf(stderr, "Logging configuration failed "
                    "(using user-defined configuration in '%s')\n", log_file);
        }
    }

    return status;
}

void finish_logging() {
    zlog_fini();
}

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
