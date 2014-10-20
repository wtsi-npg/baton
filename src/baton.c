/**
 * Copyright (c) 2013-2014 Genome Research Ltd. All rights reserved.
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
 * @file baton.c
 * @author Keith James <kdj@sanger.ac.uk>
 */

#include <errno.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jansson.h>
#include "rodsType.h"
#include "rodsErrorTable.h"
#include "rodsClient.h"
#include "miscUtil.h"

#include "baton.h"
#include "error.h"
#include "json.h"
#include "json_query.h"
#include "log.h"
#include "query.h"
#include "read.h"
#include "utilities.h"

static json_t *list_data_object(rcComm_t *conn, rodsPath_t *rods_path,
                                option_flags flags, baton_error_t *error);

static json_t *list_collection(rcComm_t *conn, rodsPath_t *rods_path,
                               option_flags flags, baton_error_t *error);

static char *slurp_file(rcComm_t *conn, rodsPath_t *rods_path,
                        size_t buffer_size, baton_error_t *error);

static const char *metadata_op_name(metadata_op operation);

static void map_mod_args(modAVUMetadataInp_t *out, mod_metadata_in_t *in);

static json_t *map_access_args(rcComm_t *conn, json_t *access,
                               baton_error_t *error);

static json_t *revmap_access_result(json_t *access,
                                    baton_error_t *error);

static const char *map_access_level(const char *access_level,
                                    baton_error_t *error);

static const char *revmap_access_level(const char *icat_level);

static int check_str_arg(const char *arg_name, const char *arg_value,
                         size_t arg_size, baton_error_t *error);


int is_irods_available() {
    rcComm_t *conn = NULL;
    rErrMsg_t errmsg;
    rodsEnv env;
    int status;

    status = getRodsEnv(&env);
    if (status < 0) {
        logmsg(ERROR, "Failed to load your iRODS environment");
        goto error;
    }

    conn = rcConnect(env.rodsHost, env.rodsPort, env.rodsUserName, env.rodsZone,
                     NO_RECONN, &errmsg);
    int available;
    if (conn) {
        available = 1;
        rcDisconnect(conn);
    }
    else {
        available = 0;
    }

    return available;

error:

    return status;
}

int declare_client_name(const char *prog_path) {
    char client_name[MAX_CLIENT_NAME_LEN];
    const char *prog_name = parse_base_name(prog_path);

    snprintf(client_name, MAX_CLIENT_NAME_LEN, "%s:%s:%s",
             PACKAGE_NAME, prog_name, VERSION);

    return setenv(SP_OPTION, client_name, 1);
}

rcComm_t *rods_login(rodsEnv *env) {
    rcComm_t *conn = NULL;
    rErrMsg_t errmsg;
    int status;

    status = getRodsEnv(env);
    if (status < 0) {
        logmsg(ERROR, "Failed to load your iRODS environment");
        goto error;
    }

    // TODO: add option for NO_RECONN vs. RECONN_TIMEOUT
    conn = rcConnect(env->rodsHost, env->rodsPort, env->rodsUserName,
                     env->rodsZone, RECONN_TIMEOUT, &errmsg);
    if (!conn) {
        logmsg(ERROR, "Failed to connect to %s:%d zone '%s' as '%s'",
            env->rodsHost, env->rodsPort, env->rodsZone, env->rodsUserName);
        goto error;
    }

    status = clientLogin(conn);
    if (status < 0) {
        logmsg(ERROR, "Failed to log in to iRODS");
        goto error;
    }

    return conn;

error:
    if (conn) rcDisconnect(conn);

    return NULL;
}

int init_rods_path(rodsPath_t *rods_path, char *inpath) {
    if (!rods_path) return USER__NULL_INPUT_ERR;

    memset(rods_path, 0, sizeof (rodsPath_t));
    char *dest = rstrcpy(rods_path->inPath, inpath, MAX_NAME_LEN);
    if (!dest) return USER_PATH_EXCEEDS_MAX;

    rods_path->objType  = UNKNOWN_OBJ_T;
    rods_path->objState = UNKNOWN_ST;

    return 0;
}

int resolve_rods_path(rcComm_t *conn, rodsEnv *env,
                      rodsPath_t *rods_path, char *inpath) {
    int status;

    status = init_rods_path(rods_path, inpath);
    if (status < 0) {
        logmsg(ERROR, "Failed to create iRODS path '%s'", inpath);
        goto error;
    }

    status = parseRodsPath(rods_path, env);
    if (status < 0) {
        logmsg(ERROR, "Failed to parse path '%s'", rods_path->inPath);
        goto error;
    }

    status = getRodsObjType(conn, rods_path);
    if (status != EXIST_ST) {
        logmsg(WARN, "Failed to stat iRODS path '%s'", rods_path->outPath);
    }

    return status;

error:
    return status;
}

int set_rods_path(rcComm_t *conn, rodsPath_t *rods_path, char *path) {
    char *dest;
    int status;

    status = init_rods_path(rods_path, path);
    if (status < 0) {
        logmsg(ERROR, "Failed to create iRODS path '%s'", path);
        goto error;
    }

    dest = rstrcpy(rods_path->outPath, path, MAX_NAME_LEN);
    if (!dest) {
        status = USER_PATH_EXCEEDS_MAX;
        goto error;
    }

    status = getRodsObjType(conn, rods_path);
    if (status != EXIST_ST) {
        logmsg(ERROR, "Failed to stat iRODS path '%s': %d", path, status);
        goto error;
    }

    return status;

error:
    return status;
}

json_t *get_user(rcComm_t *conn, const char *user_name, baton_error_t *error) {
    genQueryInp_t *query_in = NULL;
    json_t *results         = NULL;
    json_t *user            = NULL;

    query_format_in_t format =
        { .num_columns = 4,
          .columns     = { COL_USER_NAME, COL_USER_ID,
                           COL_USER_TYPE, COL_USER_ZONE },
          .labels      = { JSON_USER_NAME_KEY, JSON_USER_ID_KEY,
                           JSON_USER_TYPE_KEY, JSON_USER_ZONE_KEY } };

    check_str_arg("user_name", user_name, NAME_LEN, error);
    if (error->code != 0) goto error;

    query_in = make_query_input(SEARCH_MAX_ROWS, format.num_columns,
                                format.columns);
    query_in = prepare_user_search(query_in, user_name);

    results = do_query(conn, query_in, format.labels, error);
    if (error->code != 0) goto error;

    if (json_array_size(results) != 1) {
        set_baton_error(error, -1, "Expected 1 user result but found %d",
                        json_array_size(results));
        goto error;
    }

    user = json_incref(json_array_get(results, 0));
    json_array_clear(results);
    json_decref(results);

    if (query_in) free_query_input(query_in);

    return user;

error:
    logmsg(ERROR, error->message);

    if (query_in) free_query_input(query_in);
    if (user)     json_decref(user);
    if (results)  json_decref(results);

    return NULL;
}

json_t *list_path(rcComm_t *conn, rodsPath_t *rods_path, option_flags flags,
                  baton_error_t *error) {
    json_t *result = NULL;

    init_baton_error(error);

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

            if (flags & PRINT_CONTENTS) {
                logmsg(WARN, "Ignoring request to print the contents of data "
                       "object '%s' as if it were a collection",
                       rods_path->outPath);
            }

            result = list_data_object(conn, rods_path, flags, error);
            if (error->code != 0) goto error;

            if (flags & PRINT_ACL) {
                result = add_acl_json_object(conn, result, error);
                if (error->code != 0) goto error;
            }
            if (flags & PRINT_AVU) {
                result = add_avus_json_object(conn, result, error);
                if (error->code != 0) goto error;
            }
            if (flags & PRINT_TIMESTAMP) {
                result = add_tps_json_object(conn, result, error);
                if (error->code != 0) goto error;
            }

            break;

        case COLL_OBJ_T:
            logmsg(TRACE, "Identified '%s' as a collection",
                   rods_path->outPath);

            result = collection_path_to_json(rods_path->outPath, error);
            if (error->code != 0) goto error;

            if (flags & PRINT_ACL) {
                result = add_acl_json_object(conn, result, error);
                if (error->code != 0) goto error;
            }
            if (flags & PRINT_AVU) {
                result = add_avus_json_object(conn, result, error);
                if (error->code != 0) goto error;
            }
            if (flags & PRINT_TIMESTAMP) {
                result = add_tps_json_object(conn, result, error);
                if (error->code != 0) goto error;
            }

            if (flags & PRINT_CONTENTS) {
                json_t *contents = list_collection(conn, rods_path, flags,
                                                   error);
                if (error->code != 0) goto error;

                if (flags & PRINT_ACL) {
                    contents = add_acl_json_array(conn, contents, error);
                    if (error->code != 0) goto error;
                }
                if (flags & PRINT_AVU) {
                    contents = add_avus_json_array(conn, contents, error);
                    if (error->code != 0) goto error;
                }
                if (flags & PRINT_TIMESTAMP) {
                    contents = add_tps_json_array(conn, contents, error);
                    if (error->code != 0) goto error;
                }

                add_contents(result, contents, error);
                if (error->code != 0) goto error;
            }

            break;

        default:
            set_baton_error(error, USER_INPUT_PATH_ERR,
                            "Failed to list '%s' as it is "
                            "neither data object nor collection",
                            rods_path->outPath);
            goto error;
    }

    return result;

error:
    if (result) json_decref(result);
    logmsg(ERROR, error->message);

    return NULL;
}

json_t *ingest_path(rcComm_t *conn, rodsPath_t *rods_path,
                    option_flags flags, size_t buffer_size,
                    baton_error_t *error) {
    char *content = NULL;

    if (buffer_size == 0) {
        set_baton_error(error, -1, "Invalid buffer_size argument %u",
                        buffer_size);
        goto error;
    }

    // Currently only data objects are supported
    if (rods_path->objType != DATA_OBJ_T) {
        init_baton_error(error);
        set_baton_error(error, USER_INPUT_PATH_ERR,
                        "Cannot read the contents of '%s' because "
                        "it is not a data object", rods_path->outPath);
        goto error;
    }

    json_t *results = list_path(conn, rods_path, flags, error);
    if (error->code != 0) goto error;

    content = slurp_file(conn, rods_path, buffer_size, error);

    if (content) {
        size_t len = strlen(content);

        if (maybe_utf8(content, len)) {
            json_t *packed = json_pack("s", content);
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

int write_path_to_file(rcComm_t *conn, rodsPath_t *rods_path,
                       const char *local_path, size_t buffer_size,
                       baton_error_t *error) {
    FILE *stream = NULL;

    init_baton_error(error);

    if (buffer_size == 0) {
        set_baton_error(error, -1, "Invalid buffer_size argument %u",
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
    fclose(stream);

    return error->code;

error:
    if (stream) fclose(stream);

    return error->code;
}

int write_path_to_stream(rcComm_t *conn, rodsPath_t *rods_path, FILE *out,
                         size_t buffer_size, baton_error_t *error) {
    data_obj_file_t *obj_file = NULL;

    init_baton_error(error);

    if (buffer_size == 0) {
        set_baton_error(error, -1, "Invalid buffer_size argument %u",
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

    close_data_obj(conn, obj_file);
    free_data_obj(obj_file);

    return n;

error:
    if (obj_file) {
        close_data_obj(conn, obj_file);
        free_data_obj(obj_file);
    }

    return error->code;
}

json_t *list_permissions(rcComm_t *conn, rodsPath_t *rods_path,
                         baton_error_t *error) {
    genQueryInp_t *query_in = NULL;
    json_t *results         = NULL;

    query_format_in_t obj_format =
        { .num_columns = 2,
          .columns     = { COL_USER_NAME, COL_DATA_ACCESS_NAME },
          .labels      = { JSON_OWNER_KEY, JSON_LEVEL_KEY } };
    query_format_in_t col_format =
        { .num_columns = 2,
          .columns     = { COL_COLL_USER_NAME, COL_COLL_ACCESS_NAME },
          .labels      = { JSON_OWNER_KEY, JSON_LEVEL_KEY } };

    init_baton_error(error);

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
            query_in = make_query_input(SEARCH_MAX_ROWS, obj_format.num_columns,
                                        obj_format.columns);
            query_in = prepare_obj_acl_list(query_in, rods_path);
            break;

        case COLL_OBJ_T:
            logmsg(TRACE, "Identified '%s' as a collection",
                   rods_path->outPath);
            query_in = make_query_input(SEARCH_MAX_ROWS, col_format.num_columns,
                                        col_format.columns);
            query_in = prepare_col_acl_list(query_in, rods_path);
            break;

        default:
            set_baton_error(error, USER_INPUT_PATH_ERR,
                            "Failed to list metadata on '%s' as it is "
                            "neither data object nor collection",
                            rods_path->outPath);
            goto error;
    }

    // We need to add a zone hint to return results from other zones.
    // Without it, we will only see ACLs in the current zone. The
    // iRODS path seems to work for this purpose
    addKeyVal(&query_in->condInput, ZONE_KW, rods_path->outPath);
    logmsg(DEBUG, "Using zone hint '%s'", rods_path->outPath);
    results = do_query(conn, query_in, obj_format.labels, error);
    if (error->code != 0) goto error;

    logmsg(DEBUG, "Obtained ACL data on '%s'", rods_path->outPath);
    free_query_input(query_in);

    results = revmap_access_result(results, error);
    if (error->code != 0) goto error;

    return results;

error:
    logmsg(ERROR, "Failed to list ACL on '%s': error %d %s",
           rods_path->outPath, error->code, error->message);

    if (query_in) free_query_input(query_in);
    if (results)  json_decref(results);

    return NULL;
}

json_t *list_metadata(rcComm_t *conn, rodsPath_t *rods_path, char *attr_name,
                      baton_error_t *error) {
    genQueryInp_t *query_in = NULL;
    json_t *results         = NULL;

    query_format_in_t obj_format =
        { .num_columns  = 3,
          .columns      = { COL_META_DATA_ATTR_NAME, COL_META_DATA_ATTR_VALUE,
                            COL_META_DATA_ATTR_UNITS },
          .labels       = { JSON_ATTRIBUTE_KEY, JSON_VALUE_KEY,
                            JSON_UNITS_KEY } };

    query_format_in_t col_format =
        { .num_columns  = 3,
          .columns      = { COL_META_COLL_ATTR_NAME, COL_META_COLL_ATTR_VALUE,
                            COL_META_COLL_ATTR_UNITS },
          .labels       = { JSON_ATTRIBUTE_KEY, JSON_VALUE_KEY,
                            JSON_UNITS_KEY } };

    init_baton_error(error);

    if (attr_name) {
        check_str_arg("attr_name", attr_name, MAX_STR_LEN, error);
        if (error->code != 0) goto error;
    }

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
            query_in = make_query_input(SEARCH_MAX_ROWS, obj_format.num_columns,
                                        obj_format.columns);
            query_in = prepare_obj_list(query_in, rods_path, attr_name);
            break;

        case COLL_OBJ_T:
            logmsg(TRACE, "Identified '%s' as a collection",
                   rods_path->outPath);
            query_in = make_query_input(SEARCH_MAX_ROWS, col_format.num_columns,
                                        col_format.columns);
            query_in = prepare_col_list(query_in, rods_path, attr_name);
            break;

        default:
            set_baton_error(error, USER_INPUT_PATH_ERR,
                            "Failed to list metadata on '%s' as it is "
                            "neither data object nor collection",
                            rods_path->outPath);
            goto error;
    }

    results = do_query(conn, query_in, obj_format.labels, error);
    if (error->code != 0) goto error;

    logmsg(DEBUG, "Obtained metadata on '%s'", rods_path->outPath);
    free_query_input(query_in);

    return results;

error:
    logmsg(ERROR, "Failed to list metadata on '%s': error %d %s",
           rods_path->outPath, error->code, error->message);

    if (query_in) free_query_input(query_in);
    if (results)  json_decref(results);

    return NULL;
}

json_t *search_metadata(rcComm_t *conn, json_t *query, char *zone_name,
                        option_flags flags, baton_error_t *error) {
    json_t *results      = NULL;
    json_t *collections  = NULL;
    json_t *data_objects = NULL;
    int status;

    query_format_in_t *col_format = &(query_format_in_t)
        { .num_columns = 1,
          .columns     = { COL_COLL_NAME },
          .labels      = { JSON_COLLECTION_KEY } };

    query_format_in_t *obj_format;
    if (flags & PRINT_SIZE) {
        obj_format = &(query_format_in_t)
            { .num_columns = 3,
              .columns     = { COL_COLL_NAME, COL_DATA_NAME, COL_DATA_SIZE },
              .labels      = { JSON_COLLECTION_KEY, JSON_DATA_OBJECT_KEY,
                               JSON_SIZE_KEY },
              .latest      = 1 };
    }
    else {
        obj_format = &(query_format_in_t)
            { .num_columns = 2,
              .columns     = { COL_COLL_NAME, COL_DATA_NAME },
              .labels      = { JSON_COLLECTION_KEY, JSON_DATA_OBJECT_KEY },
              .latest      = 0 };
    }

    init_baton_error(error);

    if (zone_name) {
        check_str_arg("zone_name", zone_name, NAME_LEN, error);
        if (error->code != 0) goto error;
    }

    query = map_access_args(conn, query, error);
    if (error->code != 0) goto error;

    results = json_array();
    if (!results) {
        set_baton_error(error, -1, "Failed to allocate a new JSON array");
        goto error;
    }

    if (flags & SEARCH_COLLECTIONS) {
        logmsg(TRACE, "Searching for collections ...");
        collections = do_search(conn, zone_name, query, col_format,
                                prepare_col_avu_search, prepare_col_acl_search,
                                prepare_col_cre_search, prepare_col_mod_search,
                                error);
        if (error->code != 0) goto error;

        status = json_array_extend(results, collections);
        if (status != 0) {
            set_baton_error(error, status, "Failed to add collection results");
            goto error;
        }

        json_decref(collections);
    }

    if (flags & SEARCH_OBJECTS) {
        logmsg(TRACE, "Searching for data objects ...");
        data_objects = do_search(conn, zone_name, query, obj_format,
                                 prepare_obj_avu_search, prepare_obj_acl_search,
                                 prepare_obj_cre_search, prepare_obj_mod_search,
                                 error);
        if (error->code != 0) goto error;

        status = json_array_extend(results, data_objects);
        if (status != 0) {
            set_baton_error(error, status, "Failed to add data object results");
            goto error;
        }

        json_decref(data_objects);
    }

    if (flags & PRINT_ACL) {
        results = add_acl_json_array(conn, results, error);
        if (error->code != 0) goto error;
    }
    if (flags & PRINT_AVU) {
        results = add_avus_json_array(conn, results, error);
        if (error->code != 0) goto error;
    }
    if (flags & PRINT_TIMESTAMP) {
        results = add_tps_json_array(conn, results, error);
        if (error->code != 0) goto error;
    }

    return results;

error:
    logmsg(ERROR, error->message);

    if (results)      json_decref(results);
    if (collections)  json_decref(collections);
    if (data_objects) json_decref(data_objects);

    return NULL;
}

json_t *list_timestamps(rcComm_t *conn, rodsPath_t *rods_path,
                        baton_error_t *error) {
    genQueryInp_t *query_in = NULL;
    json_t *results         = NULL;

    query_format_in_t obj_format =
        { .num_columns = 3,
          .columns     = { COL_D_CREATE_TIME, COL_D_MODIFY_TIME,
                           COL_DATA_REPL_NUM },
          .labels      = { JSON_CREATED_KEY, JSON_MODIFIED_KEY,
                           JSON_REPLICATE_KEY } };
    query_format_in_t col_format =
        { .num_columns = 2,
          .columns     = { COL_COLL_CREATE_TIME, COL_COLL_MODIFY_TIME },
          .labels      = { JSON_CREATED_KEY, JSON_MODIFIED_KEY } };

    init_baton_error(error);

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
            query_in = make_query_input(SEARCH_MAX_ROWS, obj_format.num_columns,
                                        obj_format.columns);
            query_in = prepare_obj_tps_list(query_in, rods_path);
            break;

        case COLL_OBJ_T:
            logmsg(TRACE, "Identified '%s' as a collection",
                   rods_path->outPath);
            query_in = make_query_input(SEARCH_MAX_ROWS, col_format.num_columns,
                                        col_format.columns);
            query_in = prepare_col_tps_list(query_in, rods_path);
            break;

        default:
            set_baton_error(error, USER_INPUT_PATH_ERR,
                            "Failed to list timestamps of '%s' as it is "
                            "neither data object nor collection",
                            rods_path->outPath);
            goto error;
    }

    addKeyVal(&query_in->condInput, ZONE_KW, rods_path->outPath);
    logmsg(DEBUG, "Using zone hint '%s'", rods_path->outPath);
    results = do_query(conn, query_in, obj_format.labels, error);
    if (error->code != 0) goto error;

    logmsg(DEBUG, "Obtained timestamps of '%s'", rods_path->outPath);
    free_query_input(query_in);

    return results;

error:
    logmsg(ERROR, "Failed to list timestamps of '%s': error %d %s",
           rods_path->outPath, error->code, error->message);

    if (query_in)   free_query_input(query_in);
    if (results)    json_decref(results);

    return NULL;
}

int modify_permissions(rcComm_t *conn, rodsPath_t *rods_path,
                       recursive_op recurse, char *owner_specifier,
                       char *access_level, baton_error_t *error) {
    char user_name[NAME_LEN];
    char zone_name[NAME_LEN];
    modAccessControlInp_t mod_perms_in;
    int status;

    check_str_arg("owner specifier", owner_specifier, MAX_STR_LEN, error);
    if (error->code != 0) goto error;

    status = parseUserName(owner_specifier, user_name, zone_name);
    if (status != 0) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Failed to chmod '%s' because of an invalid "
                        "owner format '%s'",
                        rods_path->outPath, owner_specifier);
        goto error;
    }

    logmsg(DEBUG, "Parsed owner to user: '%s' zone: '%s'",
           user_name, zone_name);

    mod_perms_in.recursiveFlag = recurse;
    mod_perms_in.accessLevel   = access_level;
    mod_perms_in.userName      = user_name;
    mod_perms_in.zone          = zone_name;
    mod_perms_in.path          = rods_path->outPath;

    if (!(str_equals_ignore_case(access_level,
                                 ACCESS_LEVEL_NULL, MAX_STR_LEN) ||
          str_equals_ignore_case(access_level,
                                 ACCESS_LEVEL_OWN,  MAX_STR_LEN) ||
          str_equals_ignore_case(access_level,
                                 ACCESS_LEVEL_READ, MAX_STR_LEN) ||
          str_equals_ignore_case(access_level,
                                 ACCESS_LEVEL_WRITE, MAX_STR_LEN))) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid permission level: expected one of "
                        "[%s, %s, %s, %s]",
                        ACCESS_LEVEL_NULL, ACCESS_LEVEL_OWN,
                        ACCESS_LEVEL_READ, ACCESS_LEVEL_WRITE);
        goto error;
    }

    status = rcModAccessControl(conn, &mod_perms_in);
    if (status < 0) {
        set_baton_error(error, status, "Failed to modify permissions "
                        "of '%s' to '%s' for '%s'",
                        rods_path->outPath, access_level, owner_specifier);
        goto error;
    }

    logmsg(DEBUG, "Set permissions of '%s' to '%s' for '%s'",
           rods_path->outPath, access_level, owner_specifier);

    return error->code;

error:
    if (conn->rError) {
        logmsg(ERROR, error->message);
        log_rods_errstack(ERROR, conn->rError);
    }
    else {
        logmsg(ERROR, error->message);
    }

    return error->code;
}

int modify_json_permissions(rcComm_t *conn, rodsPath_t *rods_path,
                            recursive_op recurse, json_t *access,
                            baton_error_t *error) {
    char owner_specifier[LONG_NAME_LEN] = { 0 };
    char access_level[LONG_NAME_LEN]    = { 0 };
    const char *owner;
    const char *level;

    init_baton_error(error);

    owner = get_access_owner(access, error);
    if (error->code != 0) goto error;

    level = get_access_level(access, error);
    if (error->code != 0) goto error;

    snprintf(owner_specifier, sizeof owner_specifier, "%s", owner);
    snprintf(access_level, sizeof access_level, "%s", level);

    modify_permissions(conn, rods_path, recurse, owner_specifier,
                       access_level, error);
    return error->code;

error:
    return error->code;
}

int modify_metadata(rcComm_t *conn, rodsPath_t *rods_path,
                    metadata_op operation,
                    char *attr_name, char *attr_value, char *attr_units,
                    baton_error_t *error) {
    char *err_name;
    char *err_subname;
    char *type_arg;
    int status;

    check_str_arg("attr_name", attr_name, MAX_STR_LEN, error);
    if (error->code != 0) goto error;

    check_str_arg("attr_value", attr_value, MAX_STR_LEN, error);
    if (error->code != 0) goto error;

    // attr_units may be empty or NULL

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
            type_arg = "-d";
            break;

        case COLL_OBJ_T:
            logmsg(TRACE, "Identified '%s' as a collection",
                   rods_path->outPath);
            type_arg = "-C";
            break;

        default:
            set_baton_error(error, USER_INPUT_PATH_ERR,
                            "Failed to set metadata on '%s' as it is "
                            "neither data object nor collection",
                            rods_path->outPath);
            goto error;
    }

    mod_metadata_in_t named_args;
    named_args.op         = operation;
    named_args.type_arg   = type_arg;
    named_args.rods_path  = rods_path;
    named_args.attr_name  = attr_name;
    named_args.attr_value = attr_value;
    named_args.attr_units = attr_units;

    modAVUMetadataInp_t anon_args;
    map_mod_args(&anon_args, &named_args);
    status = rcModAVUMetadata(conn, &anon_args);
    if (status < 0) {
        err_name = rodsErrorName(status, &err_subname);
        set_baton_error(error, status,
                        "Failed to %s metadata '%s' -> '%s' on '%s': "
                        "error %d %s %s", metadata_op_name(operation),
                        attr_name, attr_value, rods_path->outPath,
                        status, err_name, err_subname);
        goto error;
    }

    return status;

error:
    if (conn->rError) {
        logmsg(ERROR, error->message);
        log_rods_errstack(ERROR, conn->rError);
    }
    else {
        logmsg(ERROR, error->message);
    }

    return error->code;
}

int modify_json_metadata(rcComm_t *conn, rodsPath_t *rods_path,
                         metadata_op operation, json_t *avu,
                         baton_error_t *error) {
    char attr_name[MAX_STR_LEN]  = { 0 };
    char attr_value[MAX_STR_LEN] = { 0 };
    char attr_units[MAX_STR_LEN] = { 0 };
    const char *attr;
    const char *value;
    const char *units;

    init_baton_error(error);

    attr = get_avu_attribute(avu, error);
    if (error->code != 0) goto error;

    value = get_avu_value(avu, error);
    if (error->code != 0) goto error;

    units = get_avu_units(avu, error);
    if (error->code != 0) goto error;

    snprintf(attr_name, sizeof attr_name, "%s", attr);
    snprintf(attr_value, sizeof attr_value, "%s", value);

    // Units are optional
    if (units) {
        snprintf(attr_units, sizeof attr_units, "%s", units);
    }

    modify_metadata(conn, rods_path, operation,
                    attr_name, attr_value, attr_units, error);

    return error->code;

error:
    return error->code;
}

static json_t *list_data_object(rcComm_t *conn, rodsPath_t *rods_path,
                                option_flags flags, baton_error_t *error) {
    genQueryInp_t *query_in = NULL;
    json_t         *results = NULL;
    json_t *data_object;
    json_t *str_size;
    size_t num_size;

    query_format_in_t *obj_format;

    if (flags & PRINT_SIZE) {
        obj_format = &(query_format_in_t)
            { .num_columns = 3,
              .columns     = { COL_COLL_NAME, COL_DATA_NAME, COL_DATA_SIZE },
              .labels      = { JSON_COLLECTION_KEY, JSON_DATA_OBJECT_KEY,
                               JSON_SIZE_KEY } };
    }
    else {
        obj_format = &(query_format_in_t)
            { .num_columns = 2,
              .columns     = { COL_COLL_NAME, COL_DATA_NAME },
              .labels      = { JSON_COLLECTION_KEY, JSON_DATA_OBJECT_KEY } };
    }

    init_baton_error(error);

    query_in = make_query_input(SEARCH_MAX_ROWS, obj_format->num_columns,
                                obj_format->columns);
    query_in = prepare_obj_list(query_in, rods_path, NULL);

    results = do_query(conn, query_in, obj_format->labels, error);
    if (error->code != 0) goto error;

    if (json_array_size(results) != 1) {
        set_baton_error(error, -1, "Expected 1 data object result but found %d",
                        json_array_size(results));
        goto error;
    }

    data_object = json_incref(json_array_get(results, 0));
    json_array_clear(results);
    json_decref(results);

    if (flags & PRINT_SIZE) {
        str_size = json_object_get(data_object, JSON_SIZE_KEY);
        num_size = atol(json_string_value(str_size));
        json_object_del(data_object, JSON_SIZE_KEY);
        json_object_set_new(data_object, JSON_SIZE_KEY, json_integer(num_size));
    }

    if (query_in) free_query_input(query_in);

    return data_object;

error:
    if (query_in) free_query_input(query_in);
    if (results)  json_decref(results);

    return NULL;
}

static json_t *list_collection(rcComm_t *conn, rodsPath_t *rods_path,
                               option_flags flags, baton_error_t *error) {
    int query_flags = DATA_QUERY_FIRST_FG;
    collHandle_t coll_handle;
    collEnt_t coll_entry;

    json_t *results = NULL;

    char *err_name;
    char *err_subname;
    int status;

    status = rclOpenCollection(conn, rods_path->outPath, query_flags,
                               &coll_handle);
    if (status < 0) {
        if (conn->rError) {
            err_name = rodsErrorName(status, &err_subname);
            set_baton_error(error, status,
                            "Failed to open collection: '%s' error %d %s %s",
                            rods_path->outPath, status, err_name, err_subname);
        }

        goto error;
    }

    results = json_array();
    if (!results) {
        set_baton_error(error, -1, "Failed to allocate a new JSON array");
        goto query_error;
    }

    while ((status = rclReadCollection(conn, &coll_handle, &coll_entry)) >= 0) {
        json_t *entry;

        switch (coll_entry.objType) {
            case DATA_OBJ_T:
                logmsg(TRACE, "Identified '%s/%s' as a data object",
                       coll_entry.collName, coll_entry.dataName);
                entry = data_object_parts_to_json(coll_entry.collName,
                                                  coll_entry.dataName, error);
                if (error->code != 0) goto query_error;

                if (flags & PRINT_SIZE) {
                    int size_status =
                        json_object_set_new(entry, JSON_SIZE_KEY,
                                            json_integer(coll_entry.dataSize));
                    if (size_status != 0) {
                        set_baton_error(error, size_status,
                                        "Failed to add data size of '%s' "
                                        "to JSON: error %d",
                                        rods_path->outPath, status);
                        goto query_error;
                    }
                }

                break;

            case COLL_OBJ_T:
                logmsg(TRACE, "Identified '%s' as a collection",
                       coll_entry.collName);
                entry = collection_path_to_json(coll_entry.collName, error);
                if (error->code != 0) goto query_error;
                break;

            default:
                set_baton_error(error, USER_INPUT_PATH_ERR,
                                "Failed to list entry '%s' in '%s' as it is "
                                "neither data object nor collection",
                                coll_entry.dataName, rods_path->outPath);
                goto query_error;
        }

        status = json_array_append_new(results, entry);
        if (status != 0) {
            set_baton_error(error, status,
                            "Failed to convert listing of '%s' to JSON: "
                            "error %d", rods_path->outPath, status);
            goto query_error;
        }
    }

    rclCloseCollection(&coll_handle); // Always returns 0 in iRODS 3.3

    return results;

query_error:
    rclCloseCollection(&coll_handle);
    if (results) json_decref(results);

error:
    if (conn->rError) {
        logmsg(ERROR, error->message);
        log_rods_errstack(ERROR, conn->rError);
    }
    else {
        logmsg(ERROR, error->message);
    }

    return NULL;
}

static char *slurp_file(rcComm_t *conn, rodsPath_t *rods_path,
                        size_t buffer_size, baton_error_t *error) {
    data_obj_file_t *obj_file = NULL;

    if (buffer_size == 0) {
        set_baton_error(error, -1, "Invalid buffer_size argument %u",
                        buffer_size);
        goto error;
    }

    logmsg(DEBUG, "Buffer size %zu", buffer_size);

    obj_file = open_data_obj(conn, rods_path, error);
    if (error->code != 0) goto error;

    char *content = slurp_data_object(conn, obj_file, buffer_size, error);
    if (error->code != 0) goto error;

    close_data_obj(conn, obj_file);
    free_data_obj(obj_file);

    return content;

error:
    if (obj_file) {
        close_data_obj(conn, obj_file);
        free_data_obj(obj_file);
    }

    return NULL;
}

static const char *metadata_op_name(metadata_op op) {
    const char *name;

    switch (op) {
        case META_ADD:
            name = META_ADD_NAME;
            break;

        case META_REM:
            name = META_REM_NAME;
            break;

        default:
            name = NULL;
    }

    return name;
}

static void map_mod_args(modAVUMetadataInp_t *out, mod_metadata_in_t *in) {
    out->arg0 = (char *) metadata_op_name(in->op);
    out->arg1 = in->type_arg;
    out->arg2 = in->rods_path->outPath;
    out->arg3 = in->attr_name;
    out->arg4 = in->attr_value;
    out->arg5 = in->attr_units;
    out->arg6 = "";
    out->arg7 = "";
    out->arg8 = "";
    out->arg9 = "";
}

static json_t *map_access_args(rcComm_t *conn, json_t *query,
                               baton_error_t *error) {
    json_t *user_info = NULL;

    if (has_acl(query)) {
        json_t *acl = get_acl(query, error);
        if (error->code != 0) goto error;

        size_t num_elts = json_array_size(acl);
        for (size_t i = 0; i < num_elts; i++) {
            json_t *access = json_array_get(acl, i);
            if (!json_is_object(access)) {
                set_baton_error(error, CAT_INVALID_ARGUMENT,
                                "Invalid access at position %d of %d: ",
                                "not a JSON object", i, num_elts);
                goto error;
            }

            // Map user name to user_id
            const char *owner_name = get_access_owner(access, error);
            if (error->code != 0) goto error;

            logmsg(DEBUG, "Getting user information for '%s'", owner_name);
            user_info = get_user(conn, owner_name, error);
            if (error->code != 0) goto error;

            json_t *user_id =
                json_incref(json_object_get(user_info, JSON_USER_ID_KEY));
            logmsg(DEBUG, "Mapping user name '%s' to user id '%s'",
                owner_name, json_string_value(user_id));

            json_object_set_new(access, JSON_OWNER_KEY, user_id);
            json_decref(user_info);

            // Map CLI access level to iCAT access type token
            const char *access_level = get_access_level(access, error);
            if (error->code != 0) goto error;

            const char *icat_level = map_access_level(access_level, error);
            if (error->code != 0) goto error;

            logmsg(DEBUG, "Mapped access level '%s' to ICAT '%s'",
                   access_level, icat_level);

            json_object_del(access, JSON_LEVEL_KEY);
            json_object_set_new(access, JSON_LEVEL_KEY,
                                json_string(icat_level));
        }
    }

    return query;

error:
    if (user_info) json_decref(user_info);

    return NULL;
}

static json_t *revmap_access_result(json_t *acl,  baton_error_t *error) {
    size_t num_elts;

    if (!json_is_array(acl)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid ACL: not a JSON array");
        goto error;
    }

    num_elts = json_array_size(acl);
    for (size_t i = 0; i < num_elts; i++) {
        json_t *access = json_array_get(acl, i);
        json_t *level = json_object_get(access, JSON_LEVEL_KEY);

        const char *icat_level = json_string_value(level);
        const char *access_level = revmap_access_level(icat_level);
        if (error->code != 0) goto error;

        logmsg(DEBUG, "Mapped ICAT '%s' to access level '%s'",
               access_level, icat_level);

        json_object_del(access, JSON_LEVEL_KEY);
        json_object_set_new(access, JSON_LEVEL_KEY,
                            json_string(access_level));
    }

    return acl;

error:
    return NULL;
}

// Map a user-visible access level to the iCAT token
// nomenclature. iRODS does a similar thing itself.
static const char *map_access_level(const char *access_level,
                                    baton_error_t *error) {
    if (str_equals_ignore_case(access_level,
                               ACCESS_LEVEL_NULL, MAX_STR_LEN)) {
        return ACCESS_NULL;
    }
    else if (str_equals_ignore_case(access_level,
                                    ACCESS_LEVEL_OWN, MAX_STR_LEN)) {
        return ACCESS_OWN;
    }
    else if (str_equals_ignore_case(access_level,
                                    ACCESS_LEVEL_READ, MAX_STR_LEN)) {
        return ACCESS_READ_OBJECT;
    }
    else if (str_equals_ignore_case(access_level,
                                    ACCESS_LEVEL_WRITE, MAX_STR_LEN)) {
        return ACCESS_MODIFY_OBJECT;
    }
    else {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid permission level: expected one of "
                        "[%s, %s, %s, %s]",
                        ACCESS_LEVEL_NULL, ACCESS_LEVEL_OWN,
                        ACCESS_LEVEL_READ, ACCESS_LEVEL_WRITE);
        return NULL;
    }
}

// Map an iCAT token back to a user-visible access level.
static const char *revmap_access_level(const char *icat_level) {
    if (str_equals_ignore_case(icat_level,
                               ACCESS_NULL, MAX_STR_LEN)) {
        return ACCESS_LEVEL_NULL;
    }
    else if (str_equals_ignore_case(icat_level,
                                    ACCESS_OWN, MAX_STR_LEN)) {
        return ACCESS_LEVEL_OWN;
    }
    else if (str_equals_ignore_case(icat_level,
                                    ACCESS_READ_OBJECT, MAX_STR_LEN)) {
        return ACCESS_LEVEL_READ;
    }
    else if (str_equals_ignore_case(icat_level,
                                    ACCESS_MODIFY_OBJECT, MAX_STR_LEN)) {
        return ACCESS_LEVEL_WRITE;
    }
    else {
        // Fall back for anything else; not ideal, but it's more
        // resilient to surprises than raising an error.
        return icat_level;
    }
}

static int check_str_arg(const char *arg_name, const char *arg_value,
                         size_t arg_size, baton_error_t *error) {
    if (!arg_value) {
        set_baton_error(error, CAT_INVALID_ARGUMENT, "%s was null", arg_name);
        goto error;
    }

    size_t len = strnlen(arg_value, MAX_STR_LEN);
    size_t term_len = len + 1;

    if (len == 0) {
        set_baton_error(error, CAT_INVALID_ARGUMENT, "%s was empty", arg_name);
        goto error;
    }
    if (term_len > arg_size) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "%s exceeded the maximum length of %d characters",
                        arg_name, arg_size);
        goto error;
    }

    return error->code;

error:
    return error->code;
}
