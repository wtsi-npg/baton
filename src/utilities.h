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

#ifndef _BATON_UTILITIES_H
#define _BATON_UTILITIES_H

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

int ends_with(const char *str, const char *suffix);

char *copy_str(const char *str);

#endif // _BATON_UTILITIES_H
