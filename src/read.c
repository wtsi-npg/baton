/**
 * Copyright (C) 2014, 2015, 2017, 2018, 2020 Genome Research Ltd. All
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
 * @file read.c
 * @author Keith James <kdj@sanger.ac.uk>
 */

#include <assert.h>

#include "config.h"
#include "compat_checksum.h"
#include "read.h"

static char *do_slurp(rcComm_t *conn, rodsPath_t *rods_path,
                      size_t buffer_size, baton_error_t *error) {
    data_obj_file_t *obj_file = NULL;
    int                 flags = 0;

    if (buffer_size == 0) {
        set_baton_error(error, -1, "Invalid buffer_size argument %zu",
                        buffer_size);
        goto error;
    }

    logmsg(DEBUG, "Using a 'slurp' buffer size of %zu bytes", buffer_size);

    obj_file = open_data_obj(conn, rods_path, O_RDONLY, flags, error);
    if (error->code != 0) goto error;

    char *content = slurp_data_obj(conn, obj_file, buffer_size, error);
    int status = close_data_obj(conn, obj_file);

    if (error->code != 0) goto error;
    if (status < 0) {
        char *err_subname;
        const char *err_name = rodsErrorName(status, &err_subname);
        set_baton_error(error, status,
                        "Failed to close data object: '%s' error %d %s",
                        rods_path->outPath, status, err_name);
        goto error;
    }

    free_data_obj(obj_file);

    return content;

error:
    if (obj_file) free_data_obj(obj_file);

    return NULL;
}

json_t *ingest_data_obj(rcComm_t *conn, rodsPath_t *rods_path,
                        option_flags flags, size_t buffer_size,
                        baton_error_t *error) {
    char *content = NULL;

    init_baton_error(error);

    if (buffer_size == 0) {
        set_baton_error(error, -1, "Invalid buffer_size argument %zu",
                        buffer_size);
        goto error;
    }

    // Currently only data objects are supported
    if (rods_path->objType != DATA_OBJ_T) {
        set_baton_error(error, USER_INPUT_PATH_ERR,
                        "Cannot read the contents of '%s' because "
                        "it is not a data object", rods_path->outPath);
        goto error;
    }

    json_t *results = list_path(conn, rods_path, flags, error);
    if (error->code != 0) goto error;

    content = do_slurp(conn, rods_path, buffer_size, error);
    if (error->code != 0) goto error;

    if (content) {
        size_t len = strlen(content);

        if (maybe_utf8(content, len)) {
            json_t *packed = json_pack("s", content);
            if (!packed) {
                set_baton_error(error, -1,
                                "Failed to pack the %zu byte contents "
                                "of '%s' as JSON", len, rods_path->outPath);
                goto error;
            }

            json_object_set_new(results, JSON_DATA_KEY, packed);
        }
        else {
            set_baton_error(error, USER_INPUT_PATH_ERR,
                            "The contents of '%s' cannot be encoded as UTF-8 "
                            "for JSON output", rods_path->outPath);
            goto error;
        }
        free(content);
    }

    return results;

error:
    if (content) free(content);

    return NULL;
}

data_obj_file_t *open_data_obj(rcComm_t *conn, rodsPath_t *rods_path,
                               int open_flag, int flags,
                               baton_error_t *error) {
    data_obj_file_t *data_obj = NULL;
    dataObjInp_t obj_open_in;
    int descriptor;

    init_baton_error(error);

    memset(&obj_open_in, 0, sizeof obj_open_in);

    logmsg(DEBUG, "Opening data object '%s'", rods_path->outPath);
    snprintf(obj_open_in.objPath, MAX_NAME_LEN, "%s", rods_path->outPath);

    if (flags & WRITE_LOCK) {
      logmsg(DEBUG, "Enabling write lock for '%s'", rods_path->outPath);
      addKeyVal(&obj_open_in.condInput, LOCK_TYPE_KW, WRITE_LOCK_TYPE);
    }

    switch(open_flag) {
        case (O_RDONLY):
          obj_open_in.openFlags = O_RDONLY;

          descriptor = rcDataObjOpen(conn, &obj_open_in);
          break;

        case (O_WRONLY):
          obj_open_in.openFlags  = O_WRONLY;
          obj_open_in.createMode = 0750;
          obj_open_in.dataSize   = 0;
          addKeyVal(&obj_open_in.condInput, FORCE_FLAG_KW, "");
          descriptor = rcDataObjCreate(conn, &obj_open_in);
          clearKeyVal(&obj_open_in.condInput);
          break;

        default:
          set_baton_error(error, -1,
                          "Failed to open '%s': file open flag must be either"
                          "O_RDONLY or O_WRONLY", rods_path->outPath,
                          open_flag);
          goto error;
    }

    if (descriptor < 0) {
        char *err_subname;
        const char *err_name = rodsErrorName(descriptor, &err_subname);
        set_baton_error(error, descriptor,
                        "Failed to open '%s': error %d %s",
                        rods_path->outPath, descriptor, err_name);
        goto error;
    }

    data_obj = calloc(1, sizeof (data_obj_file_t));
    if (!data_obj) {
        logmsg(ERROR, "Failed to allocate memory: error %d %s",
               errno, strerror(errno));
        goto error;
    }

    data_obj->path                = rods_path->outPath;
    data_obj->flags               = obj_open_in.openFlags;
    data_obj->open_obj            = calloc(1, sizeof (openedDataObjInp_t));
    data_obj->open_obj->l1descInx = descriptor;
    data_obj->md5_last_read       = calloc(33, sizeof (char));
    data_obj->md5_last_write      = calloc(33, sizeof (char));

    return data_obj;

error:
    if (data_obj) free_data_obj(data_obj);

    return NULL;
}

int close_data_obj(rcComm_t *conn, data_obj_file_t *data_obj) {
    logmsg(DEBUG, "Closing '%s'", data_obj->path);
    int status = rcDataObjClose(conn, data_obj->open_obj);

    return status;
}

void free_data_obj(data_obj_file_t *data_obj) {
    assert(data_obj);

    if (data_obj->open_obj)       free(data_obj->open_obj);
    if (data_obj->md5_last_read)  free(data_obj->md5_last_read);
    if (data_obj->md5_last_write) free(data_obj->md5_last_write);

    free(data_obj);
}

size_t read_chunk(rcComm_t *conn, data_obj_file_t *data_obj, char *buffer,
                  size_t len, baton_error_t *error) {
    init_baton_error(error);

    data_obj->open_obj->len = len;

    bytesBuf_t obj_read_out;
    memset(&obj_read_out, 0, sizeof obj_read_out);
    obj_read_out.buf = buffer;
    obj_read_out.len = len;

    logmsg(DEBUG, "Reading up to %zu bytes from '%s'", len, data_obj->path);

    int num_read = rcDataObjRead(conn, data_obj->open_obj, &obj_read_out);
    if (num_read < 0) {
        char *err_subname;
        const char *err_name = rodsErrorName(num_read, &err_subname);
        set_baton_error(error, num_read,
                        "Failed to read up to %zu bytes from '%s': %s",
                        len, data_obj->path, err_name);
        goto finally;
    }

    logmsg(DEBUG, "Read %d bytes from '%s'", num_read, data_obj->path);

finally:
    return num_read;
}

size_t read_data_obj(rcComm_t *conn, data_obj_file_t *data_obj,
                     FILE *out, size_t buffer_size, baton_error_t *error) {
    size_t num_read    = 0;
    size_t num_written = 0;
    char *buffer       = NULL;

    init_baton_error(error);

    if (buffer_size == 0) {
        set_baton_error(error, -1, "Invalid buffer_size argument %u",
                        buffer_size);
        goto finally;
    }

    buffer = calloc(buffer_size +1, sizeof (char));
    if (!buffer) {
        logmsg(ERROR, "Failed to allocate memory: error %d %s",
               errno, strerror(errno));
        goto finally;
    }

    unsigned char digest[16];
    MD5_CTX context;
    compat_MD5Init(&context);

    size_t nr, nw;
    while ((nr = read_chunk(conn, data_obj, buffer, buffer_size, error)) > 0) {
        num_read += nr;
        logmsg(DEBUG, "Writing %zu bytes from '%s' to stream",
               nr, data_obj->path);

        int status = fwrite(buffer, 1, nr, out);
        if (status < 0) {
            logmsg(ERROR, "Failed to write to stream: error %d %s",
                   errno, strerror(errno));
            goto finally;
        }
        nw = nr;
        num_written += nw;

        compat_MD5Update(&context, (unsigned char*) buffer, nr);
        memset(buffer, 0, buffer_size);
    }

    compat_MD5Final(digest, &context);
    set_md5_last_read(data_obj, digest);

    if (num_read != num_written) {
        set_baton_error(error, -1, "Read %zu bytes from '%s' but wrote "
                        "%zu bytes ", num_read, data_obj->path, num_written);
        goto finally;
    }

    if (!validate_md5_last_read(conn, data_obj)) {
        logmsg(WARN, "Checksum mismatch for '%s' having MD5 %s on reading",
               data_obj->path, data_obj->md5_last_read);
    }

    logmsg(NOTICE, "Wrote %zu bytes from '%s' to stream having MD5 %s",
           num_written, data_obj->path, data_obj->md5_last_read);

finally:
    if (buffer) free(buffer);

    return num_written;
}

char *slurp_data_obj(rcComm_t *conn, data_obj_file_t *data_obj,
                     size_t buffer_size, baton_error_t *error) {
    char *buffer  = NULL;
    char *content = NULL;

    init_baton_error(error);

    buffer = calloc(buffer_size +1, sizeof (char));
    if (!buffer) {
        logmsg(ERROR, "Failed to allocate memory: error %d %s",
               errno, strerror(errno));
        goto error;
    }

    unsigned char digest[16];
    MD5_CTX context;
    compat_MD5Init(&context);

    size_t capacity = buffer_size;
    size_t num_read = 0;

    content = calloc(capacity, sizeof (char));
    if (!content) {
        logmsg(ERROR, "Failed to allocate memory: error %d %s",
               errno, strerror(errno));
        goto error;
    }

    size_t nr;
    while ((nr = read_chunk(conn, data_obj, buffer, buffer_size, error)) > 0) {
      logmsg(TRACE, "Read %zu bytes. Capacity %zu, num read %zu",
             nr, capacity, num_read);
        if (num_read + nr > capacity) {
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

        memcpy(content + num_read, buffer, nr);
        memset(buffer, 0, buffer_size);
        num_read += nr;
    }

    logmsg(DEBUG, "Final capacity %zu, offset %zu", capacity, num_read);

    compat_MD5Update(&context, (unsigned char *) content, num_read);
    compat_MD5Final(digest, &context);
    set_md5_last_read(data_obj, digest);

    if (!validate_md5_last_read(conn, data_obj)) {
        logmsg(WARN, "Checksum mismatch for '%s' having MD5 %s on reading",
               data_obj->path, data_obj->md5_last_read);
    }

    logmsg(NOTICE, "Wrote %zu bytes from '%s' to buffer having MD5 %s",
           num_read, data_obj->path, data_obj->md5_last_read);

    if (buffer) free(buffer);

    return content;

error:
    if (buffer)  free(buffer);
    if (content) free(content);

    return NULL;
}

int get_data_obj_file(rcComm_t *conn, rodsPath_t *rods_path,
                      const char *local_path, size_t buffer_size,
                      baton_error_t *error) {
    FILE *stream = NULL;

    init_baton_error(error);

    if (buffer_size == 0) {
        set_baton_error(error, -1, "Invalid buffer_size argument %zu",
                        buffer_size);
        goto finally;
    }

    logmsg(DEBUG, "Writing '%s' to '%s'", rods_path->outPath, local_path);

    // Currently only data objects are supported
    if (rods_path->objType != DATA_OBJ_T) {
        set_baton_error(error, USER_INPUT_PATH_ERR,
                        "Cannot write the contents of '%s' because "
                        "it is not a data object", rods_path->outPath);
        goto finally;
    }

    stream = fopen(local_path, "w");
    if (!stream) {
        set_baton_error(error, errno,
                        "Failed to open '%s' for writing: error %d %s",
                        local_path, errno, strerror(errno));
        goto finally;
    }

    get_data_obj_stream(conn, rods_path, stream, buffer_size, error);
    int status = fclose(stream);

    if (error->code != 0) goto finally;
    if (status != 0) {
        set_baton_error(error, errno,
                        "Failed to close '%s': error %d %s",
                        local_path, errno, strerror(errno));
    }

finally:
    return error->code;
}

int get_data_obj_stream(rcComm_t *conn, rodsPath_t *rods_path, FILE *out,
                        size_t buffer_size, baton_error_t *error) {
    data_obj_file_t *data_obj = NULL;
    int                 flags = 0;

    init_baton_error(error);

    if (buffer_size == 0) {
        set_baton_error(error, -1, "Invalid buffer_size argument %zu",
                        buffer_size);
        goto error;
    }

    logmsg(DEBUG, "Writing '%s' to a stream", rods_path->outPath);

    if (rods_path->objType != DATA_OBJ_T) {
        set_baton_error(error, USER_INPUT_PATH_ERR,
                        "Cannot write the contents of '%s' because "
                        "it is not a data object", rods_path->outPath);
        goto error;
    }

    data_obj = open_data_obj(conn, rods_path, O_RDONLY, flags, error);
    if (error->code != 0) goto error;

    size_t nr = read_data_obj(conn, data_obj, out, buffer_size, error);
    int status = close_data_obj(conn, data_obj);

    if (error->code != 0) goto error;
    if (status < 0) {
        char *err_subname;
        const char *err_name = rodsErrorName(status, &err_subname);
        set_baton_error(error, status,
                        "Failed to close data object: '%s' error %d %s",
                        rods_path->outPath, status, err_name);
        goto error;
    }

    free_data_obj(data_obj);

    return nr;

error:
    if (data_obj) free_data_obj(data_obj);

    return error->code;
}

json_t *checksum_data_obj(rcComm_t *conn, rodsPath_t *rods_path,
                          option_flags flags, baton_error_t *error) {
    char *checksum_str = NULL;
    json_t *checksum   = NULL;

    dataObjInp_t obj_chk_in;
    int status;

    init_baton_error(error);

    memset(&obj_chk_in, 0, sizeof obj_chk_in);
    obj_chk_in.openFlags = O_RDONLY;

    if (rods_path->objState == NOT_EXIST_ST) {
        set_baton_error(error, USER_FILE_DOES_NOT_EXIST,
                        "Path '%s' does not exist "
                        "(or lacks access permission)", rods_path->outPath);
        goto error;
    }

    switch (rods_path->objType) {
        case DATA_OBJ_T:
            logmsg(TRACE, "Identified '%s' as a data object",
                   rods_path->outPath);
            snprintf(obj_chk_in.objPath, MAX_NAME_LEN, "%s",
                     rods_path->outPath);
            break;

        case COLL_OBJ_T:
            logmsg(TRACE, "Identified '%s' as a collection",
                   rods_path->outPath);
            set_baton_error(error, USER_INPUT_PATH_ERR,
                            "Failed to list checksum of '%s' as it is "
                            "a collection", rods_path->outPath);
            goto error;

        default:
            set_baton_error(error, USER_INPUT_PATH_ERR,
                            "Failed to list checksum of '%s' as it is "
                            "neither data object nor collection",
                            rods_path->outPath);
            goto error;
    }

    if (flags & CALCULATE_CHECKSUM) {
        logmsg(DEBUG, "Calculating checksums of all replicates "
               "of data object '%s'", rods_path->outPath);
        addKeyVal(&obj_chk_in.condInput, CHKSUM_ALL_KW, "");

        if (flags & FORCE) {
            logmsg(DEBUG, "Forcing checksum recaclulation "
                   "of data object '%s'", rods_path->outPath);
            addKeyVal(&obj_chk_in.condInput, FORCE_CHKSUM_KW, "");
        }
    }

    status = rcDataObjChksum(conn, &obj_chk_in, &checksum_str);
    clearKeyVal(&obj_chk_in.condInput);

    if (status < 0) {
        char *err_subname;
        const char *err_name = rodsErrorName(status, &err_subname);
        set_baton_error(error, status,
                        "Failed to list checksum of '%s': %d %s",
                        rods_path->outPath, status, err_name);
        goto error;
    }

    checksum = json_pack("s", checksum_str);
    if (!checksum) {
        set_baton_error(error, -1, "Failed to pack checksum '%s' as JSON",
                        checksum_str);
        goto error;
    }

    if (checksum_str) free(checksum_str);

    return checksum;

error:
    if (checksum_str) free(checksum_str);
    if (checksum) json_decref(checksum);

    return NULL;
}

void set_md5_last_read(data_obj_file_t *data_obj, unsigned char digest[16]) {
    char *md5 = data_obj->md5_last_read;
    for (int i = 0; i < 16; i++) {
        snprintf(md5 + i * 2, 3, "%02x", digest[i]);
    }
}

int validate_md5_last_read(rcComm_t *conn, data_obj_file_t *data_obj) {
    dataObjInp_t obj_md5_in;
    memset(&obj_md5_in, 0, sizeof obj_md5_in);

    snprintf(obj_md5_in.objPath, MAX_NAME_LEN, "%s", data_obj->path);

    char *md5 = NULL;
    int status = rcDataObjChksum(conn, &obj_md5_in, &md5);
    if (status < 0) goto finally;

    logmsg(DEBUG, "Comparing last read MD5 of '%s' with expected MD5 of '%s'",
           data_obj->md5_last_read, md5);

    status = str_equals_ignore_case(data_obj->md5_last_read, md5, 32);

finally:
    if (md5) free(md5);

    return status;
}
