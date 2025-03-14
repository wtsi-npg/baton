/**
 * Copyright (C) 2014, 2015, 2017, 2018, 2019, 2020, 2021, 2025 Genome
 * Research Ltd. All rights reserved.
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

// Workaround for the accidental removal of client-side checksum API
// from iRODS in iRODS 4.1.x https://github.com/irods/irods/issues/5731

#if IRODS_VERSION_INTEGER <= (4*1000000 + 2*1000 + 9)
int chksumLocFile( const char *fileName, char *chksumStr, const char* );
#else
#include <checksum.h>
#endif

#include "config.h"
#include "compat_checksum.h"
#include "write.h"

int put_data_obj(rcComm_t *conn, const char *local_path, rodsPath_t *rods_path,
                 char *default_resource, char *checksum, const int flags,
                 baton_error_t *error) {
    char *tmpname  = NULL;
    dataObjInp_t obj_open_in;
    int status;

    init_baton_error(error);

    memset(&obj_open_in, 0, sizeof obj_open_in);

    logmsg(DEBUG, "Opening data object '%s'", rods_path->outPath);
    snprintf(obj_open_in.objPath, MAX_NAME_LEN, "%s", rods_path->outPath);

    tmpname = copy_str(local_path, MAX_STR_LEN);

    obj_open_in.openFlags  = O_WRONLY;
    obj_open_in.createMode = 0750;

    // Set to 0 when we dont know the size. However, this means that
    // rcDataObjPut will check for a local file on disk to get the
    // size.
    obj_open_in.dataSize   = 0;

    if ((flags & VERIFY_CHECKSUM) && (flags & CALCULATE_CHECKSUM)) {
        set_baton_error(error, USER_INPUT_OPTION_ERR,
                        "Cannot both verify and update the checksum "
                        "when putting data object '%s'", rods_path->outPath);
        goto error;
    }

    if (flags & VERIFY_CHECKSUM) {
	char chksum[NAME_LEN];

	if (checksum) {
	    snprintf(chksum, NAME_LEN, "%s", checksum);
	    logmsg(DEBUG, "Using supplied local checksum '%s' for '%s'",
		   chksum, rods_path->outPath);
	}
	else {
	    // The hash scheme must be defined for rcChksumLocFile, but if
	    // it is zero length, rcChksumLocFile falls back to the value
	    // in the client environment. There's no advantage in our
	    // passing in a value that we have read from the client
	    // environment.
	    const char* default_scheme = "";
	    status = chksumLocFile(tmpname, chksum, default_scheme);
	    if (status != 0) {
		char *err_subname;
		const char *err_name = rodsErrorName(status, &err_subname);
		set_baton_error(error, status,
				"Failed to calculate a local checksum for: '%s' "
				"error %d %s", rods_path->outPath, status,
				err_name);
		goto error;
	    }
	    logmsg(DEBUG, "Calculated a local checksum '%s' for '%s'",
		   chksum, rods_path->outPath);
	}
	
        logmsg(DEBUG, "Server will verify '%s' after put",
	       rods_path->outPath);
        addKeyVal(&obj_open_in.condInput, VERIFY_CHKSUM_KW, chksum);
    }
    else if (flags & CALCULATE_CHECKSUM) {
        logmsg(DEBUG, "Server will calculate checksum for '%s'",
               rods_path->outPath);
        addKeyVal(&obj_open_in.condInput, REG_CHKSUM_KW, "");
    }

    if (flags & WRITE_LOCK) {
      logmsg(DEBUG, "Enabling put write lock for '%s'", rods_path->outPath);
      addKeyVal(&obj_open_in.condInput, LOCK_TYPE_KW, WRITE_LOCK_TYPE);
    }
    if (default_resource) {
        logmsg(DEBUG, "Using '%s' as the default iRODS resource",
               default_resource);
        addKeyVal(&obj_open_in.condInput, DEF_RESC_NAME_KW, default_resource);
    }

    // Always force put over any existing data in order to make puts
    // idempotent.
    addKeyVal(&obj_open_in.condInput, FORCE_FLAG_KW, "");

    status = rcDataObjPut(conn, &obj_open_in, tmpname);
    if (status < 0) {
        char *err_subname;
        const char *err_name = rodsErrorName(status, &err_subname);
        set_baton_error(error, status,
                        "Failed to put data object: '%s' error %d %s",
                        rods_path->outPath, status, err_name);
        goto error;
    }
    logmsg(NOTICE, "Put '%s' to '%s'", tmpname, rods_path->outPath);

    free(tmpname);

    return error->code;

error:
    if (tmpname) free(tmpname);

    return error->code;
}

size_t write_data_obj(rcComm_t *conn, FILE *in, rodsPath_t *rods_path,
                      const size_t buffer_size, const int flags, baton_error_t *error) {
    data_obj_file_t *obj = NULL;
    char *buffer         = NULL;
    size_t num_read      = 0;
    size_t num_written   = 0;

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

    obj = open_data_obj(conn, rods_path, O_WRONLY, flags, error);
    if (error->code != 0) goto finally;

    unsigned char digest[16];
    EVP_MD_CTX *context = compat_MD5Init(error);
    if (error->code != 0) {
        logmsg(ERROR, error->message);
        goto finally;
    }

    size_t nr;
    while ((nr = fread(buffer, 1, buffer_size, in)) > 0) {
        num_read += nr;
        logmsg(DEBUG, "Writing %zu bytes from stream to '%s'", nr, obj->path);

        const size_t nw = write_chunk(conn, buffer, obj, nr, error);
        if (error->code != 0) {
            logmsg(ERROR, "Failed to write to '%s': error %d %s",
                   obj->path, error->code, error->message);
            goto finally;
        }
        num_written += nw;

        compat_MD5Update(context, (unsigned char*) buffer, nr, error);
        if (error->code != 0) {
            logmsg(ERROR, error->message);
            goto finally;
        }
        memset(buffer, 0, buffer_size);
    }

    compat_MD5Final(digest, context, error);
    if (error->code != 0) {
        logmsg(ERROR, error->message);
        goto finally;
    }
    set_md5_last_read(obj, digest);

    const int status = close_data_obj(conn, obj);
    if (status < 0) {
        char *err_subname;
        const char *err_name = rodsErrorName(status, &err_subname);
        set_baton_error(error, status,
                        "Failed to close data object: '%s' error %d %s",
                        obj->path, status, err_name);
        goto finally;
    }

    if (num_read != num_written) {
        set_baton_error(error, -1, "Read %zu bytes but wrote %zu bytes ",
                        "to '%s'", num_read, num_written, obj->path);
        goto finally;
    }

    if (!validate_md5_last_read(conn, obj)) {
        logmsg(WARN, "Checksum mismatch for '%s' having MD5 %s on reading",
               obj->path, obj->md5_last_read);
    }

    logmsg(NOTICE, "Wrote %zu bytes to '%s' having MD5 %s",
           num_written, obj->path, obj->md5_last_read);

finally:
    if (obj)    free_data_obj(obj);
    if (buffer) free(buffer);

    return num_written;
}

size_t write_chunk(rcComm_t *conn, char *buffer, const data_obj_file_t *data_obj,
                   const size_t len, baton_error_t *error) {
    init_baton_error(error);

    data_obj->open_obj->len = len;

    bytesBuf_t obj_write_in;
    memset(&obj_write_in, 0, sizeof obj_write_in);
    obj_write_in.buf = buffer;
    obj_write_in.len = len;

    int num_written = rcDataObjWrite(conn, data_obj->open_obj, &obj_write_in);
    if (num_written < 0) {
        char *err_subname;
        const char *err_name = rodsErrorName(num_written, &err_subname);
        set_baton_error(error, num_written,
                        "Failed to write %zu bytes to '%s': %s",
                        len, data_obj->path, err_name);
        goto finally;
    }

    logmsg(DEBUG, "Wrote %d bytes to '%s'", num_written, data_obj->path);

finally:
    return num_written;
}

int create_collection(rcComm_t *conn, rodsPath_t *rods_path, int flags,
                      baton_error_t *error) {
    collInp_t coll_create_in;

    init_baton_error(error);

    memset(&coll_create_in, 0, sizeof coll_create_in);

    snprintf(coll_create_in.collName, MAX_NAME_LEN, "%s", rods_path->outPath);
    if (flags & RECURSIVE) {
        logmsg(DEBUG, "Creating collection '%s' recursively",
               rods_path->outPath);
        addKeyVal(&coll_create_in.condInput, RECURSIVE_OPR__KW, "");
    }

    int status = rcCollCreate(conn, &coll_create_in);
    if (status < 0) {
        char *err_subname;
        const char *err_name = rodsErrorName(status, &err_subname);
        set_baton_error(error, status,
                        "Failed to put create collection: '%s' error %d %s",
                        rods_path->outPath, status, err_name);
    }

    return error->code;
}

int remove_data_object(rcComm_t *conn, rodsPath_t *rods_path, int flags,
                       baton_error_t *error) {
    dataObjInp_t obj_rm_in;
    flags = flags;

    init_baton_error(error);

    memset(&obj_rm_in, 0, sizeof obj_rm_in);

    logmsg(DEBUG, "Removing data object '%s'", rods_path->outPath);
    snprintf(obj_rm_in.objPath, MAX_NAME_LEN, "%s", rods_path->outPath);

    addKeyVal(&obj_rm_in.condInput, FORCE_FLAG_KW, "");

    const int status = rcDataObjUnlink(conn, &obj_rm_in);
    if (status < 0) {
        char *err_subname;
        const char *err_name = rodsErrorName(status, &err_subname);
        set_baton_error(error, status,
                        "Failed to remove data object: '%s' error %d %s",
                        rods_path->outPath, status, err_name);
    }

    return error->code;
}

int remove_collection(rcComm_t *conn, rodsPath_t *rods_path, const int flags,
                      baton_error_t *error) {
    init_baton_error(error);

    collInp_t col_rm_in;
    memset(&col_rm_in, 0, sizeof col_rm_in);

    logmsg(DEBUG, "Removing collection '%s'", rods_path->outPath);
    snprintf(col_rm_in.collName, MAX_NAME_LEN, "%s", rods_path->outPath);

    if (flags & RECURSIVE) {
        logmsg(DEBUG, "Enabling recursive removal of '%s'", rods_path->outPath);
        addKeyVal(&col_rm_in.condInput, RECURSIVE_OPR__KW, "");
    }
    if (flags & FORCE) {
        logmsg(DEBUG, "Enabling forced removal of '%s'", rods_path->outPath);
        addKeyVal(&col_rm_in.condInput, FORCE_FLAG_KW, "");
    }

    const int verbose = 0;
    const int status = rcRmColl(conn, &col_rm_in, verbose);
    if (status < 0) {
        char *err_subname;
        const char *err_name = rodsErrorName(status, &err_subname);
        set_baton_error(error, status,
                        "Failed to remove collection: '%s' error %d %s",
                        rods_path->outPath, status, err_name);
    }

    return error->code;
}
