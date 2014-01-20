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
 * @file utilities.h
 * @author Keith James <kdj@sanger.ac.uk>
 */

#ifndef _BATON_UTILITIES_H
#define _BATON_UTILITIES_H

#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#include <zlog.h>

enum {
    ZLOG_LEVEL_TRACE = 30
};

#define vzlog_trace(cat, format, args) \
    vzlog(cat, __FILE__, sizeof(__FILE__) -1, __func__, \
          sizeof(__func__) -1, __LINE__, ZLOG_LEVEL_TRACE, format, args)

#define BATON_CAT "baton"

/**
 *  @enum log_level
 *  @brief Log message levels.
 */
typedef enum {
    FATAL,
    ERROR,
    WARN,
    NOTICE,
    INFO,
    TRACE,
    DEBUG
} log_level;

/**
 * Log an error through the underlying logging mechanism.  This
 * function exists to abstract the logging implementation.
 *
 * @param[in] level    The logging level.
 * @param[in] category The log message category.  Categories are based
 * on e.g. program subsystem.
 * @param[in] format    The logging format string or template.
 * @param[in] arguments The format arguments.
 */
void logmsg(log_level level, const char *category, const char *format, ...);

int str_starts_with(const char *str, const char *prefix);

int str_ends_with(const char *str, const char *suffix);

int str_equals(const char *str1, const char *str2);

int str_equals_ignore_case(const char *str1, const char *str2);

char *copy_str(const char *str);

FILE *maybe_stdin(const char *path);

#endif // _BATON_UTILITIES_H
