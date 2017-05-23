/**
 * Copyright (C) 2014, 2015, 2016, 2017 Genome Research Ltd. All
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
 * @file read.h
 * @author Keith James <kdj@sanger.ac.uk>
 */

#ifndef _BATON_READ_H
#define _BATON_READ_H

#include <rodsClient.h>

#include "config.h"
#include "list.h"

/**
 *  @struct data_obj_file
 *  @brief Data object handle.
 */
typedef struct data_obj_file {
    /** The path in iRODS of the data object */
    const char *path;
    /** The data object open flags, as in dataObjInp_t .openFlags */
    int flags;
    /** Opened data object handle */
    openedDataObjInp_t *open_obj;
    /** The MD5 calculated last time the object was read completely */
    char *md5_last_read;
    /** The MD5 calculated last time the object was written completely */
    char *md5_last_write;
} data_obj_file_t;

/**
 * Open a data object for reading or writing.
 *
 * @param[in]  conn       An open iRODS connection.
 * @param[in]  rods_path  An iRODS data object path.
 * @param[in]  flags      O_RDONLY or O_WRONLY.
 * @parem[out] error      An error report struct.
 *
 * @return A new struct, which must be freed by the caller.
 */
data_obj_file_t *open_data_obj(rcComm_t *conn, rodsPath_t *rods_path,
                               int flags, baton_error_t *error);

int close_data_obj(rcComm_t *conn, data_obj_file_t *obj_file);

void free_data_obj(data_obj_file_t *obj_file);

/**
 * Read bytes from a data object into a buffer.
 *
 * @param[in]  conn       An open iRODS connection.
 * @param[in]  obj_file   A data object handle.
 * @param[in]  buffer     A buffer to read into.
 * @param[in]  len        The number of bytes to read.
 * @param[out] error      An error report struct.
 *
 * @return The number of bytes actually read, which may be 0.
 */
size_t read_chunk(rcComm_t *conn, data_obj_file_t *obj_file,
                  char *buffer, size_t len, baton_error_t *error);

/**
 * Read a data object and write to a stream.
 *
 * @param[in]  conn        An open iRODS connection.
 * @param[in]  obj_file    A data object handle.
 * @param[in]  out         A file to write to.
 * @param[in]  buffer_size The number of bytes to copy at one time.
 * @param[out] error       An error report struct.
 *
 * @return The number of bytes copied in total.
 */
size_t read_data_obj(rcComm_t *conn, data_obj_file_t *obj_file, FILE *out,
                     size_t buffer_size, baton_error_t *error);

/**
 * Read a data object to a new byte string.
 *
 * @param[in]  conn        An open iRODS connection.
 * @param[in]  obj_file    A data object handle.
 * @param[in]  buffer_size The number of bytes to copy at a time while filling
                           the new buffer.
 * @param[out] error       An error report struct.
 *
 * @return A new byte string containing the entire data object, which must be
 *         freed by the caller.
 */
char *slurp_data_obj(rcComm_t *conn, data_obj_file_t *obj_file,
                     size_t buffer_size, baton_error_t *error);

json_t *ingest_data_obj(rcComm_t *conn, rodsPath_t *rods_path,
                        option_flags flags,
                        size_t buffer_size, baton_error_t *error);

int get_data_obj_file(rcComm_t *conn, rodsPath_t *rods_path,
                      const char *local_path, size_t buffer_size,
                      baton_error_t *error);

int get_data_obj_stream(rcComm_t *conn, rodsPath_t *rods_path, FILE *out,
                        size_t buffer_size, baton_error_t *error);

json_t *checksum_data_obj(rcComm_t *conn, rodsPath_t *rods_path,
                          option_flags flags, baton_error_t *error);

void set_md5_last_read(data_obj_file_t *obj_file, unsigned char digest[16]);

int validate_md5_last_read(rcComm_t *conn, data_obj_file_t *obj_file);

#endif // _BATON_READ_H
