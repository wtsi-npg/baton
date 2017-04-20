/**
 * Copyright (C) 2014, 2015 Genome Research Ltd. All rights reserved.
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
 * @file write.c
 * @author Keith James <kdj@sanger.ac.uk>
 */

#include "config.h"
#include "write.h"

int write_path_to_file(rcComm_t *conn, rodsPath_t *rods_path,
                       const char *local_path, size_t buffer_size,
                       baton_error_t *error) {
    FILE *stream = NULL;

    init_baton_error(error);

    if (buffer_size == 0) {
        set_baton_error(error, -1, "Invalid buffer_size argument %zu",
                        buffer_size);
        goto error;
    }

    logmsg(DEBUG, "Writing '%s' to '%s'", rods_path->outPath, local_path);

    // Currently only data objects are supported
    if (rods_path->objType != DATA_OBJ_T) {
        set_baton_error(error, USER_INPUT_PATH_ERR,
                        "Cannot write the contents of '%s' because "
                        "it is not a data object", rods_path->outPath);
        goto error;
    }

    stream = fopen(local_path, "w");
    if (!stream) {
        set_baton_error(error, errno,
                        "Failed to open '%s' for writing: error %d %s",
                        local_path, errno, strerror(errno));
        goto error;
    }

    write_path_to_stream(conn, rods_path, stream, buffer_size, error);
    int status = fclose(stream);

    if (error->code != 0) goto error;
    if (status != 0) {
        set_baton_error(error, errno,
                        "Failed to close '%s': error %d %s",
                        local_path, errno, strerror(errno));
        goto error;
    }

    return error->code;

error:
    return error->code;
}

int write_path_to_stream(rcComm_t *conn, rodsPath_t *rods_path, FILE *out,
                         size_t buffer_size, baton_error_t *error) {
    data_obj_file_t *obj_file = NULL;

    init_baton_error(error);

    if (buffer_size == 0) {
        set_baton_error(error, -1, "Invalid buffer_size argument %zu",
                        buffer_size);
        goto error;
    }

    logmsg(DEBUG, "Writing '%s' to a stream", rods_path->outPath);

    // Currently only data objects are supported
    if (rods_path->objType != DATA_OBJ_T) {
        set_baton_error(error, USER_INPUT_PATH_ERR,
                        "Cannot write the contents of '%s' because "
                        "it is not a data object", rods_path->outPath);
        goto error;
    }

    obj_file = open_data_obj(conn, rods_path, error);
    if (error->code != 0) goto error;

    size_t n = stream_data_object(conn, obj_file, out, buffer_size, error);
    int status = close_data_obj(conn, obj_file);

    if (error->code != 0) goto error;
    if (status < 0) {
        char *err_subname;
        char *err_name = rodsErrorName(status, &err_subname);
        set_baton_error(error, status,
                        "Failed to close data object: '%s' error %d %s",
                        rods_path->outPath, status, err_name);
        goto error;
    }

    free_data_obj(obj_file);

    return n;

error:
    if (obj_file) free_data_obj(obj_file);

    return error->code;
}
