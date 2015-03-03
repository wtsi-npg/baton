/**
 * Copyright (c) 2014, 2015 Genome Research Ltd. All rights reserved.
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
 * @file read.c
 * @author Keith James <kdj@sanger.ac.uk>
 */

#include <assert.h>

#include "config.h"

#ifdef HAVE_IRODS3
#include "dataObjOpen.h"
#include "dataObjRead.h"
#include "dataObjChksum.h"
#include "dataObjClose.h"
#endif

#ifdef HAVE_IRODS4
#include "dataObjOpen.hpp"
#include "dataObjRead.hpp"
#include "dataObjChksum.hpp"
#include "dataObjClose.hpp"
#endif

#include "error.h"
#include "read.h"
#include "utilities.h"

data_obj_file_t *open_data_obj(rcComm_t *conn, rodsPath_t *rods_path,
                               baton_error_t *error) {
    dataObjInp_t obj_open_in;
    memset(&obj_open_in, 0, sizeof obj_open_in);
    obj_open_in.openFlags = O_RDONLY;

    logmsg(DEBUG, "Opening data object '%s'", rods_path->outPath);

    snprintf(obj_open_in.objPath, MAX_NAME_LEN, "%s", rods_path->outPath);
    int descriptor = rcDataObjOpen(conn, &obj_open_in);

    if (descriptor < 0) {
        char *err_subname;
        char *err_name = rodsErrorName(descriptor, &err_subname);
        set_baton_error(error, descriptor,
                        "Failed to open '%s': %s %s", rods_path->outPath,
                        err_name, err_subname);
        goto error;
    }

    data_obj_file_t *obj_file = NULL;

    obj_file = calloc(1, sizeof (data_obj_file_t));
    if (!obj_file) {
        logmsg(ERROR, "Failed to allocate memory: error %d %s",
               errno, strerror(errno));
        goto error;
    }

    obj_file->path           = rods_path->outPath;
    obj_file->flags          = obj_open_in.openFlags;
    obj_file->descriptor     = descriptor;
    obj_file->md5_last_read  = calloc(33, sizeof (char));
    obj_file->md5_last_write = calloc(33, sizeof (char));

    return obj_file;

error:
    return NULL;
}

size_t read_data_obj(rcComm_t *conn, data_obj_file_t *obj_file, char *buffer,
                     size_t len, baton_error_t *error) {
    openedDataObjInp_t obj_read_in;
    memset(&obj_read_in, 0, sizeof obj_read_in);
    obj_read_in.l1descInx = obj_file->descriptor;
    obj_read_in.len       = len;

    bytesBuf_t obj_read_out;
    memset(&obj_read_out, 0, sizeof obj_read_out);
    obj_read_out.buf = buffer;
    obj_read_out.len = len;

    logmsg(DEBUG, "Reading up to %zu bytes from '%s'", len, obj_file->path);

    int num_read = rcDataObjRead(conn, &obj_read_in, &obj_read_out);
    if (num_read < 0) {
        char *err_subname;
        char *err_name = rodsErrorName(num_read, &err_subname);
        set_baton_error(error, num_read,
                        "Failed to read up to %zu bytes from '%s': %s %s",
                        len, obj_file->path, err_name, err_subname);
        goto error;
    }

    logmsg(DEBUG, "Read %d bytes from '%s'", num_read, obj_file->path);
    
    return num_read;

error:
    return num_read;
}

char *slurp_data_object(rcComm_t *conn, data_obj_file_t *obj_file,
                        size_t buffer_size, baton_error_t *error) {
    char *buffer  = NULL;
    char *content = NULL;

    buffer = calloc(buffer_size +1, sizeof (char));
    if (!buffer) {
        logmsg(ERROR, "Failed to allocate memory: error %d %s",
               errno, strerror(errno));
        goto error;
    }

    unsigned char digest[16];
    MD5_CTX context;
    MD5Init(&context);

    size_t capacity = buffer_size;
    size_t num_read = 0;

    content = calloc(capacity, sizeof (char));
    if (!content) {
        logmsg(ERROR, "Failed to allocate memory: error %d %s",
               errno, strerror(errno));
        goto error;
    }

    size_t n;
    while ((n = read_data_obj(conn, obj_file, buffer, buffer_size,
                              error)) > 0) {
      logmsg(TRACE, "Read %zu bytes. Capacity %zu, num read %zu",
	     n, capacity, num_read);
        if (num_read + n > capacity) {
            capacity = capacity * 2;

            char *tmp = NULL;
            tmp = realloc(content, capacity);
            if (!tmp) {
                logmsg(ERROR, "Failed to allocate memory: error %d %s",
                       errno, strerror(errno));
                goto error;
            }

            memset(tmp + num_read, 0, capacity - num_read);
            content = tmp;
        }

        memcpy(content + num_read, buffer, n);
	memset(buffer, 0, buffer_size);
        num_read += n;
    }

    logmsg(DEBUG, "Final capacity %zu, offset %zu", capacity, num_read);

    MD5Update(&context, (unsigned char*) content, num_read);
    MD5Final(digest, &context);
    set_md5_last_read(obj_file, digest);

    if (!validate_md5_last_read(conn, obj_file)) {
        logmsg(WARN, "Checksum mismatch for '%s' having MD5 %s on reading",
               obj_file->path, obj_file->md5_last_read);
    }

    logmsg(NOTICE, "Wrote %zu bytes from '%s' to buffer having MD5 %s",
           num_read, obj_file->path, obj_file->md5_last_read);

    if (buffer) free(buffer);

    return content;

error:
    if (buffer)  free(buffer);
    if (content) free(content);

    return NULL;
}

size_t stream_data_object(rcComm_t *conn, data_obj_file_t *obj_file, FILE *out,
                          size_t buffer_size, baton_error_t *error) {
    size_t num_written = 0;
    char *buffer = NULL;

    if (buffer_size == 0) {
        set_baton_error(error, -1, "Invalid buffer_size argument %u",
                        buffer_size);
        goto error;
    }

    buffer = calloc(buffer_size +1, sizeof (char));
    if (!buffer) {
        logmsg(ERROR, "Failed to allocate memory: error %d %s",
               errno, strerror(errno));
        goto error;
    }

    unsigned char digest[16];
    MD5_CTX context;
    MD5Init(&context);

    size_t n;
    while ((n = read_data_obj(conn, obj_file, buffer, buffer_size,
                              error)) > 0) {
        logmsg(DEBUG, "Writing %zu bytes from '%s' to stream",
	       n, obj_file->path);

        int status = fwrite(buffer, 1, n, out);
        if (status < 0) {
            logmsg(ERROR, "Failed to write to stream: error %d %s",
                   errno, strerror(errno));
            goto error;
        }

        MD5Update(&context, (unsigned char*) buffer, n);
	memset(buffer, 0, buffer_size);
        num_written += n;
    }

    MD5Final(digest, &context);
    set_md5_last_read(obj_file, digest);

    if (!validate_md5_last_read(conn, obj_file)) {
        logmsg(WARN, "Checksum mismatch for '%s' having MD5 %s on reading",
               obj_file->path, obj_file->md5_last_read);
    }

    logmsg(NOTICE, "Wrote %zu bytes from '%s' to stream having MD5 %s",
           num_written, obj_file->path, obj_file->md5_last_read);

    if (buffer) free(buffer);

    return num_written;

error:
    if (buffer) free(buffer);

    return num_written;
}

void set_md5_last_read(data_obj_file_t *obj_file, unsigned char digest[16]) {
    char *md5 = obj_file->md5_last_read;
    for (int i = 0; i < 16; i++) {
        snprintf(md5 + i * 2, 3, "%02x", digest[i]);
    }
}

int validate_md5_last_read(rcComm_t *conn, data_obj_file_t *obj_file) {
    dataObjInp_t obj_md5_in;
    memset(&obj_md5_in, 0, sizeof obj_md5_in);

    snprintf(obj_md5_in.objPath, MAX_NAME_LEN, "%s", obj_file->path);

    char *md5 = NULL;
    int status = rcDataObjChksum(conn, &obj_md5_in, &md5);
    if (status < 0) goto error;

    logmsg(DEBUG, "Comparing last read MD5 of '%s' with expected MD5 of '%s'",
           obj_file->md5_last_read, md5);

    status = str_equals_ignore_case(obj_file->md5_last_read, md5, 32);
    free(md5);

    return status;

error:
    if (md5) free(md5);

    return status;
}

int close_data_obj(rcComm_t *conn, data_obj_file_t *obj_file) {
    openedDataObjInp_t obj_close_in;
    memset(&obj_close_in, 0, sizeof obj_close_in);
    obj_close_in.l1descInx = obj_file->descriptor;

    logmsg(DEBUG, "Closing '%s'", obj_file->path);
    int status = rcDataObjClose(conn, &obj_close_in);

    return status;
}

void free_data_obj(data_obj_file_t *obj_file) {
    assert(obj_file);

    if (obj_file->md5_last_read)  free(obj_file->md5_last_read);
    if (obj_file->md5_last_write) free(obj_file->md5_last_write);

    free(obj_file);
}
