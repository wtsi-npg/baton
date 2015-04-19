/**
 * Copyright (C) 2013, 2014 Genome Research Ltd. All rights reserved.
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
 * @file error.c
 * @author Keith James <kdj@sanger.ac.uk>
 */

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "error.h"

void init_baton_error(baton_error_t *error) {
    assert(error);
    error->message[0] = '\0';
    error->code = 0;
    error->size = 1;
}

void set_baton_error(baton_error_t *error, int code, const char *format, ...) {
    va_list args;
    va_start(args, format);

    if (error) {
        vsnprintf(error->message, MAX_ERROR_MESSAGE_LEN, format, args);
        error->size = strnlen(error->message, MAX_ERROR_MESSAGE_LEN);
        error->code = code;
    }

    va_end(args);
}
