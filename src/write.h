/**
 * Copyright (C) 2014, 2015, 2016 Genome Research Ltd. All rights
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
 * @file write.h
 * @author Keith James <kdj@sanger.ac.uk>
 */

#ifndef _BATON_WRITE_H
#define _BATON_WRITE_H

#include <rodsClient.h>

#include "config.h"
#include "read.h"
#include "write.h"

int write_path_to_file(rcComm_t *conn, rodsPath_t *rods_path,
                       const char *local_path, size_t buffer_size,
                       baton_error_t *error);

int write_path_to_stream(rcComm_t *conn, rodsPath_t *rods_path, FILE *out,
                         size_t buffer_size, baton_error_t *error);

#endif // _BATON_WRITE_H
