/**
 * Copyright (C) 2013, 2014, 2017 Genome Research Ltd. All rights
 * reserved.
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
 * @file log.h
 * @author Keith James <kdj@sanger.ac.uk>
 */

#ifndef _BATON_LOG_H
#define _BATON_LOG_H

#include <stdarg.h>

/**
 *  @enum log_level
 *  @brief Log message levels.
 */
typedef enum {
    FATAL  = 0,
    ERROR  = 1,
    WARN   = 2,
    NOTICE = 3,
    INFO   = 4,
    DEBUG  = 5,
    TRACE  = 6
} log_level;

#define logmsg(level, ...) \
    log_impl(__LINE__, __FILE__, __func__, level, __VA_ARGS__);

void log_impl(int line, const char *file, char const *function,
              log_level level, ...);

log_level get_log_threshold();

log_level set_log_threshold(log_level level);

const char *get_log_level_name(log_level level);

#endif  // _BATON_LOG_H
