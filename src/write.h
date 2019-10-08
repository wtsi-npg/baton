/**
 * Copyright (C) 2014, 2015, 2016, 2017, 2018 Genome Research Ltd. All
 * rights reserved.
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

/**
 * Write to a data object from a local file using the put protocol.
 *
 * @param[in]  conn        An open iRODS connection.
 * @param[in]  obj_file    A local file name.
 * @param[in]  rods_path   An iRODS data object path.
 * @param[in]  flags       CALCULATE_CHECKSUM to calculate a checksum on
                           the server side. WRITE_LOCK to use an advisory
                           lock server-side. Optional.
 * @param[out] error       An error report struct.
 *
 * @return The number of bytes copied in total.
 */
int put_data_obj(rcComm_t *conn, const char *local_path, rodsPath_t *rods_path,
                 int flags, baton_error_t *error);

/**
 * Write bytes from a buffer into a data object.
 *
 * @param[in]  conn       An open iRODS connection.
 * @param[in]  buffer     A buffer to write from.
 * @param[in]  obj_file   A data object handle.
 * @param[in]  len        The number of bytes to write.
 * @param[out] error      An error report struct.
 *
 * @return The number of bytes actually written, which may be 0.
 */
size_t write_chunk(rcComm_t *conn, char *buffer, data_obj_file_t *obj_file,
                   size_t len, baton_error_t *error);

/**
 * Write to a data object from a stream.
 *
 * @param[in]  conn        An open iRODS connection.
 * @param[in]  in          File to read from.
 * @param[in]  rods_path   An iRODS data object path.
 * @param[in]  buffer_size The number of bytes to copy at one time.
 * @param[in]  flags       WRITE_LOCK to use an advisory lock server-side.
                           Optional.
 * @param[out] error       An error report struct.
 *
 * @return The number of bytes copied in total.
 */
size_t write_data_obj(rcComm_t *conn, FILE *in, rodsPath_t *rods_path,
                      size_t buffer_size, int flags, baton_error_t *error);

int remove_data_object(rcComm_t *conn, rodsPath_t *rods_path, int flags,
                      baton_error_t *error);

int create_collection(rcComm_t *conn, rodsPath_t *rods_path, int flags,
                      baton_error_t *error);

int remove_collection(rcComm_t *conn, rodsPath_t *rods_path, int flags,
                      baton_error_t *error);

#endif // _BATON_WRITE_H
