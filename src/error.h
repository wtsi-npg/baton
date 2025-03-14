/**
 * Copyright (C) 2013, 2014, 2025 Genome Research Ltd. All rights reserved.
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
 * @file error.h
 * @author Keith James <kdj@sanger.ac.uk>
 */

#ifndef _BATON_ERROR_H
#define _BATON_ERROR_H

#define MAX_ERROR_MESSAGE_LEN 1024

#include <stddef.h>

typedef struct baton_error {
    /** Error code */
    int code;
    /** Error message */
    char message[MAX_ERROR_MESSAGE_LEN];
    /** Error message length, including terminating null byte */
    size_t size;
} baton_error_t;

/**
 * Initialise an error struct before use.
 *
 * @param[in] error     A new JSON error state.
 *
 * @ref set_baton_error
 */
void init_baton_error(baton_error_t *error);

/**
 * Set error state information. The size field will be set to the
 * length of the formatted message.
 *
 * @param[in] error      The error struct to modify.
 * @param[in] code       The error code.
 * @param[in] format     The error message format string or template.
 * @param[in] ...        The format arguments.
 *
 * @ref init_baton_error
 */
void set_baton_error(baton_error_t *error, int code,
                     const char *format, ...);

#endif // _BATON_ERROR_H
