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

#include <assert.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jansson.h>
#include <zlog.h>
#include "rodsType.h"
#include "rodsErrorTable.h"
#include "rodsClient.h"
#include "miscUtil.h"

#include "baton.h"
#include "json.h"
#include "utilities.h"

typedef genQueryInp_t *(*prepare_search_cb) (genQueryInp_t *query_in,
                                             const char *attr_name,
                                             const char *attr_value,
                                             const char *operator);

static const char *metadata_op_name(metadata_op operation);

static void map_mod_args(modAVUMetadataInp_t *out, struct mod_metadata_in *in);

static genQueryInp_t *prepare_user_search(genQueryInp_t *query_in,
                                          const char *user_name);

static genQueryInp_t *prepare_obj_list(genQueryInp_t *query_in,
                                       rodsPath_t *rods_path,
                                       const char *attr_name);

static genQueryInp_t *prepare_col_list(genQueryInp_t *query_in,
                                       rodsPath_t *rods_path,
                                       const char *attr_name);

static genQueryInp_t *prepare_obj_acl_list(genQueryInp_t *query_in,
                                           rodsPath_t *rods_path);

static genQueryInp_t *prepare_col_acl_list(genQueryInp_t *query_in,
                                           rodsPath_t *rods_path);

static genQueryInp_t *prepare_obj_acl_search(genQueryInp_t *query_in,
                                             rodsPath_t *rods_path,
                                             const char *user_id,
                                             const char *perm_id);

static genQueryInp_t *prepare_obj_search(genQueryInp_t *query_in,
                                         const char *attr_name,
                                         const char *attr_value,
                                         const char *operator);

static genQueryInp_t *prepare_col_search(genQueryInp_t *query_in,
                                         const char *attr_name,
                                         const char *attr_value,
                                         const char *operator);

static genQueryInp_t *prepare_path_search(genQueryInp_t *query_in,
                                          const char *root_path);

static genQueryInp_t *prepare_json_search(genQueryInp_t *query_in, json_t *avus,
                                          prepare_search_cb prepare,
                                          baton_error_t *error);

static json_t *do_search(rcComm_t *conn, char *zone_name, json_t *query,
                         int num_columns, const int columns[],
                         const char *labels[], prepare_search_cb prepare,
                         baton_error_t *error);

static json_t *list_collection(rcComm_t *conn, rodsPath_t *rods_path,
                               baton_error_t *error);

static json_t *add_acl_json_array(rcComm_t *conn, json_t *target,
                                  baton_error_t *error);

static json_t *add_acl_json_object(rcComm_t *conn, json_t *target,
                                   baton_error_t *error);

void log_rods_errstack(log_level level, const char *category, rError_t *error) {
    rErrMsg_t *errmsg;

    int len = error->len;
    for (int i = 0; i < len; i++) {
	    errmsg = error->errMsg[i];
        logmsg(level, category, "Level %d: %s", i, errmsg->msg);
    }
}

void log_json_error(log_level level, const char *category,
                    json_error_t *error) {
    logmsg(level, category, "JSON error: %s, line %d, column %d, position %d",
           error->text, error->line, error->column, error->position);
}

void init_baton_error(baton_error_t *error) {
    assert(error);
    error->message[0] = '\0';
    error->code = 0;
    error->size = 1;
}

void set_baton_error(baton_error_t *error, int code,
                     const char *format, ...) {
    va_list args;
    va_start(args, format);

    if (error) {
        vsnprintf(error->message, MAX_ERROR_MESSAGE_LEN, format, args);
        error->size = strnlen(error->message, MAX_ERROR_MESSAGE_LEN);
        error->code = code;
    }

    va_end(args);
}

int is_irods_available() {
    rodsEnv env;
    int status;
    rErrMsg_t errmsg;

    status = getRodsEnv(&env);
    if (status < 0) {
        logmsg(ERROR, BATON_CAT, "Failed to load your iRODS environment");
        goto error;
    }

    rcComm_t *conn = rcConnect(env.rodsHost, env.rodsPort, env.rodsUserName,
                               env.rodsZone, NO_RECONN, &errmsg);
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

rcComm_t *rods_login(rodsEnv *env) {
    int status;
    rErrMsg_t errmsg;
    rcComm_t *conn = NULL;

    status = getRodsEnv(env);
    if (status < 0) {
        logmsg(ERROR, BATON_CAT, "Failed to load your iRODS environment");
        goto error;
    }

    // TODO: add option for NO_RECONN vs. RECONN_TIMEOUT
    conn = rcConnect(env->rodsHost, env->rodsPort, env->rodsUserName,
                     env->rodsZone, RECONN_TIMEOUT, &errmsg);
    if (!conn) {
        logmsg(ERROR, BATON_CAT, "Failed to connect to %s:%d zone '%s' as '%s'",
               env->rodsHost, env->rodsPort, env->rodsZone, env->rodsUserName);
        goto error;
    }

    status = clientLogin(conn);
    if (status < 0) {
        logmsg(ERROR, BATON_CAT, "Failed to log in to iRODS");
        goto error;
    }

    return conn;

error:
    if (conn) rcDisconnect(conn);

    return NULL;
}

int init_rods_path(rodsPath_t *rodspath, char *inpath) {
    if (!rodspath) return USER__NULL_INPUT_ERR;

    memset(rodspath, 0, sizeof (rodsPath_t));
    char *dest = rstrcpy(rodspath->inPath, inpath, MAX_NAME_LEN);
    if (!dest) return USER_PATH_EXCEEDS_MAX;

    return 0;
}

int resolve_rods_path(rcComm_t *conn, rodsEnv *env,
                      rodsPath_t *rods_path, char *inpath) {
    int status;

    status = init_rods_path(rods_path, inpath);
    if (status < 0) {
        logmsg(ERROR, BATON_CAT, "Failed to create iRODS path '%s'", inpath);
        goto error;
    }

    status = parseRodsPath(rods_path, env);
    if (status < 0) {
        logmsg(ERROR, BATON_CAT, "Failed to parse path '%s'",
               rods_path->inPath);
        goto error;
    }

    status = getRodsObjType(conn, rods_path);
    if (status < 0) {
        logmsg(ERROR, BATON_CAT, "Failed to stat iRODS path '%s'",
               rods_path->inPath);
        goto error;
    }

    return status;

error:
    return status;
}

int set_rods_path(rcComm_t *conn, rodsPath_t *rods_path, char *path) {
    int status;

    status = init_rods_path(rods_path, path);
    if (status < 0) {
        logmsg(ERROR, BATON_CAT, "Failed to create iRODS path '%s'", path);
        goto error;
    }

    char *dest = rstrcpy(rods_path->outPath, path, MAX_NAME_LEN);
    if (!dest) {
        status = USER_PATH_EXCEEDS_MAX;
        goto error;
    }

    status = getRodsObjType(conn, rods_path);
    if (status < 0) {
        logmsg(ERROR, BATON_CAT, "Failed to stat iRODS path '%s'", path);
        goto error;
    }

    return status;

error:
    return status;
}

json_t *get_user(rcComm_t *conn, char *user_name, baton_error_t *error) {
    const char *labels[] = { JSON_USER_NAME_KEY,
                             JSON_USER_ID_KEY,
                             JSON_USER_TYPE_KEY,
                             JSON_USER_ZONE_KEY };
    int num_cols = 4;
    int max_rows = 10;
    int cols[] = { COL_USER_NAME, COL_USER_ID, COL_USER_TYPE, COL_USER_ZONE };

    genQueryInp_t *query_in = NULL;
    json_t *results = NULL;
    json_t *user = NULL;
    init_baton_error(error);

    query_in = make_query_input(max_rows, num_cols, cols);
    query_in = prepare_user_search(query_in, user_name);

    results = do_query(conn, query_in, labels, error);
    if (error->code != 0) goto error;

    if (json_array_size(results) != 1) {
        set_baton_error(error, -1,
                        "Expected 1 user result but found %d",
                        json_array_size(results));
        goto error;
    }

    user = json_incref(json_array_get(results, 0));
    json_array_clear(results);
    json_decref(results);

    return user;

error:
    logmsg(ERROR, BATON_CAT, error->message);

    if (user) json_decref(user);
    if (results) json_decref(results);

    return NULL;
}

json_t *list_path(rcComm_t *conn, rodsPath_t *rods_path,
                  baton_error_t *error) {
    json_t *results = NULL;
    init_baton_error(error);

    if (rods_path->objState == NOT_EXIST_ST) {
        set_baton_error(error, USER_FILE_DOES_NOT_EXIST,
                        "Path '%s' does not exist "
                        "(or lacks access permission)", rods_path->outPath);
        goto error;
    }

    switch (rods_path->objType) {
        case DATA_OBJ_T:
            logmsg(TRACE, BATON_CAT, "Identified '%s' as a data object",
                   rods_path->outPath);
            results = data_object_path_to_json(rods_path->outPath);
            if (error->code != 0) goto error;

            results = add_acl_json_object(conn, results, error);
            if (error->code != 0) goto error;

            break;

        case COLL_OBJ_T:
            logmsg(TRACE, BATON_CAT, "Identified '%s' as a collection",
                   rods_path->outPath);
            results = list_collection(conn, rods_path, error);
            if (error->code != 0) goto error;

            results = add_acl_json_array(conn, results, error);
            if (error->code != 0) goto error;

            break;

        default:
            set_baton_error(error, USER_INPUT_PATH_ERR,
                            "Failed to list '%s' as it is "
                            "neither data object nor collection",
                            rods_path->outPath);
            goto error;
    }

    return results;

error:
    if (results) json_decref(results);

    return NULL;
}

json_t *list_permissions(rcComm_t *conn, rodsPath_t *rods_path,
                         baton_error_t *error) {
    const char *labels[] = { JSON_OWNER_KEY, JSON_LEVEL_KEY };

    int num_cols = 2;
    int max_rows = 10;
    int cols[num_cols];

    genQueryInp_t *query_in = NULL;
    json_t *results = NULL;
    init_baton_error(error);

    if (rods_path->objState == NOT_EXIST_ST) {
        set_baton_error(error, USER_FILE_DOES_NOT_EXIST,
                        "Path '%s' does not exist "
                        "(or lacks access permission)", rods_path->outPath);
        goto error;
    }

    switch (rods_path->objType) {
        case DATA_OBJ_T:
            logmsg(TRACE, BATON_CAT, "Identified '%s' as a data object",
                   rods_path->outPath);
            cols[0] = COL_USER_NAME;
            cols[1] = COL_DATA_ACCESS_NAME;
            query_in = make_query_input(max_rows, num_cols, cols);
            query_in = prepare_obj_acl_list(query_in, rods_path);
            break;

        case COLL_OBJ_T:
            logmsg(TRACE, BATON_CAT, "Identified '%s' as a collection",
                   rods_path->outPath);
            cols[0] = COL_COLL_USER_NAME;
            cols[1] = COL_COLL_ACCESS_NAME;
            query_in = make_query_input(max_rows, num_cols, cols);
            query_in = prepare_col_acl_list(query_in, rods_path);
            break;

        default:
            set_baton_error(error, USER_INPUT_PATH_ERR,
                            "Failed to list metadata on '%s' as it is "
                            "neither data object nor collection",
                            rods_path->outPath);
            goto error;
    }

    results = do_query(conn, query_in, labels, error);
    if (error->code != 0) goto error;

    logmsg(DEBUG, BATON_CAT, "Obtained ACL data on '%s'", rods_path->outPath);
    free_query_input(query_in);

    return results;

error:
    logmsg(ERROR, BATON_CAT, "Failed to list ACL on '%s': error %d %s",
           rods_path->outPath, error->code, error->message);

    if (query_in) free_query_input(query_in);
    if (results) json_decref(results);

    return NULL;
}

json_t *list_metadata(rcComm_t *conn, rodsPath_t *rods_path, char *attr_name,
                      baton_error_t *error) {
    const char *labels[] = { JSON_ATTRIBUTE_KEY,
                             JSON_VALUE_KEY,
                             JSON_UNITS_KEY };
    int num_cols = 3;
    int max_rows = 10;
    int cols[num_cols];

    genQueryInp_t *query_in = NULL;
    json_t *results = NULL;
    init_baton_error(error);

    if (rods_path->objState == NOT_EXIST_ST) {
        set_baton_error(error, USER_FILE_DOES_NOT_EXIST,
                        "Path '%s' does not exist "
                        "(or lacks access permission)", rods_path->outPath);
        goto error;
    }

    switch (rods_path->objType) {
        case DATA_OBJ_T:
            logmsg(TRACE, BATON_CAT, "Identified '%s' as a data object",
                   rods_path->outPath);
            cols[0] = COL_META_DATA_ATTR_NAME;
            cols[1] = COL_META_DATA_ATTR_VALUE;
            cols[2] = COL_META_DATA_ATTR_UNITS;
            query_in = make_query_input(max_rows, num_cols, cols);
            query_in = prepare_obj_list(query_in, rods_path, attr_name);
            break;

        case COLL_OBJ_T:
            logmsg(TRACE, BATON_CAT, "Identified '%s' as a collection",
                   rods_path->outPath);
            cols[0] = COL_META_COLL_ATTR_NAME;
            cols[1] = COL_META_COLL_ATTR_VALUE;
            cols[2] = COL_META_COLL_ATTR_UNITS;
            query_in = make_query_input(max_rows, num_cols, cols);
            query_in = prepare_col_list(query_in, rods_path, attr_name);
            break;

        default:
            set_baton_error(error, USER_INPUT_PATH_ERR,
                            "Failed to list metadata on '%s' as it is "
                            "neither data object nor collection",
                            rods_path->outPath);
            goto error;
    }

    results = do_query(conn, query_in, labels, error);
    if (error->code != 0) goto error;

    logmsg(DEBUG, BATON_CAT, "Obtained metadata on '%s'", rods_path->outPath);
    free_query_input(query_in);

    return results;

error:
    logmsg(ERROR, BATON_CAT, "Failed to list metadata on '%s': error %d %s",
           rods_path->outPath, error->code, error->message);

    if (query_in) free_query_input(query_in);
    if (results) json_decref(results);

    return NULL;
}

json_t *search_metadata(rcComm_t *conn, json_t *query, char *zone_name,
                        baton_error_t *error) {
    int num_col_cols = 1;
    int num_obj_cols = 2;
    const char *labels[] = { JSON_COLLECTION_KEY, JSON_DATA_OBJECT_KEY };
    int cols[] = { COL_COLL_NAME, COL_DATA_NAME };
    init_baton_error(error);

    if (!json_is_object(query)) {
        set_baton_error(error, -1, "Invalid query: not a JSON object");
        goto error;
    }

    json_t *results = json_array();
    if (!results) {
        set_baton_error(error, -1, "Failed to allocate a new JSON array");
        goto error;
    }

    logmsg(TRACE, BATON_CAT, "Searching for collections ...");
    json_t *collections = do_search(conn, zone_name, query, num_col_cols, cols,
                                    labels, prepare_col_search, error);
    if (error->code != 0) goto error;

    logmsg(TRACE, BATON_CAT, "Searching for data objects ...");
    json_t *data_objects = do_search(conn, zone_name, query, num_obj_cols, cols,
                                     labels, prepare_obj_search, error);
    if (error->code != 0) goto error;

    json_array_extend(results, collections); // TODO: check return value
    json_decref(collections);
    json_array_extend(results, data_objects); // TODO: check return value
    json_decref(data_objects);

    results = add_acl_json_array(conn, results, error);
    if (error->code != 0) goto error;

    return results;

error:
    logmsg(ERROR, BATON_CAT, error->message);

    if (results)      json_decref(results);
    if (collections)  json_decref(collections);
    if (data_objects) json_decref(data_objects);

    return NULL;
}

static json_t *do_search(rcComm_t *conn, char *zone_name, json_t *query,
                         int num_columns, const int columns[],
                         const char *labels[], prepare_search_cb prepare,
                         baton_error_t *error) {
    // This appears to be the maximum number of rows returned per
    // result chunk
    int max_rows = 10;
    genQueryInp_t *query_in = NULL;

    query_in = make_query_input(max_rows, num_columns, columns);

    json_t *root_path = json_object_get(query, JSON_COLLECTION_KEY);
    if (root_path) {
        if (!json_is_string(root_path)) {
            set_baton_error(error, -1, "Invalid root path: not a JSON string");
            goto error;
        }
        query_in = prepare_path_search(query_in, json_string_value(root_path));
    }

    json_t *avus = json_object_get(query, JSON_AVUS_KEY);
    if (!json_is_array(avus)) {
        set_baton_error(error, -1, "Invalid AVUs: not a JSON array");
        goto error;
    }

    query_in = prepare_json_search(query_in, avus, prepare, error);
    if (error->code != 0) goto error;

    if (zone_name) {
        logmsg(TRACE, BATON_CAT, "Setting zone to '%s'", zone_name);
        addKeyVal(&query_in->condInput, ZONE_KW, zone_name);
    }

    json_t *items = do_query(conn, query_in, labels, error);
    if (error->code != 0) goto error;

    free_query_input(query_in);
    logmsg(TRACE, BATON_CAT, "Found %d matching items", json_array_size(items));

    return items;

error:
    logmsg(ERROR, BATON_CAT, error->message);

    if (query_in) free_query_input(query_in);
    if (items) json_decref(items);

    return NULL;
}

static genQueryInp_t *prepare_json_search(genQueryInp_t *query_in,
                                          json_t *avus,
                                          prepare_search_cb prepare,
                                          baton_error_t *error) {
    int num_clauses = json_array_size(avus);

    for (int i = 0; i < num_clauses; i++) {
        const json_t *avu = json_array_get(avus, i);
        if (!json_is_object(avu)) {
            set_baton_error(error, -1, "Invalid AVU at position %d of %d: not "
                            "a JSON object", i, num_clauses);
            goto error;
        }

        json_t *attr  = json_object_get(avu, JSON_ATTRIBUTE_KEY);
        json_t *value = json_object_get(avu, JSON_VALUE_KEY);
        json_t *oper  = json_object_get(avu, JSON_OPERATOR_KEY);

        if (!json_is_string(attr)) {
            set_baton_error(error, -1, "Invalid attribute: not a JSON string");
            goto error;
        }
        if (!json_is_string(value)) {
            set_baton_error(error, -1, "Invalid value: not a JSON string");
            goto error;
        }
        if (oper && !json_is_string(oper)) {
            set_baton_error(error, -1, "Invalid operator: not a JSON string");
            goto error;
        }

        const char *attr_name  = json_string_value(attr);
        const char *attr_value = json_string_value(value);
        const char *attr_oper  = oper ? json_string_value(oper) :
            SEARCH_OP_EQUALS;

        if (str_equals_ignore_case(attr_oper, SEARCH_OP_EQUALS) ||
            str_equals_ignore_case(attr_oper, SEARCH_OP_LIKE)) {
            prepare(query_in, attr_name, attr_value, attr_oper);
        }
        else {
            set_baton_error(error, -1, "Invalid operator: expected one of ["
                            "%s, %s]", SEARCH_OP_EQUALS, SEARCH_OP_LIKE);
        }
     }

    return query_in;

error:
    logmsg(ERROR, BATON_CAT, error->message);

    return NULL;
}

int modify_permissions(rcComm_t *conn, rodsPath_t *rods_path,
                       recursive_op recurse, char *owner_specifier,
                       char *access_level, baton_error_t *error) {
    int status;
    char user_name[NAME_LEN];
    char zone_name[NAME_LEN];
    modAccessControlInp_t mod_perms_in;

    status = parseUserName(owner_specifier, user_name, zone_name);
    if (status != 0) {
        set_baton_error(error, -1, "Failed to chmod '%s' because of an invalid "
                        "owner format '%s'", rods_path->outPath,
                        owner_specifier);
        goto error;
    }

    logmsg(DEBUG, BATON_CAT, "Parsed owner to user: '%s' zone: '%s'",
           user_name, zone_name);

    mod_perms_in.recursiveFlag = recurse;
    mod_perms_in.accessLevel   = access_level;
    mod_perms_in.userName      = user_name;
    mod_perms_in.zone          = zone_name;
    mod_perms_in.path          = rods_path->outPath;

    if (str_equals_ignore_case(access_level, ACCESS_OWN)   ||
        str_equals_ignore_case(access_level, ACCESS_WRITE) ||
        str_equals_ignore_case(access_level, ACCESS_READ)  ||
        str_equals_ignore_case(access_level, ACCESS_NULL)) {
        status = rcModAccessControl(conn, &mod_perms_in);
    }
    else {
        set_baton_error(error, -1, "Invalid permission level: expected one of ["
                        "%s, %s, %s, %s]",
                        ACCESS_OWN, ACCESS_WRITE,
                        ACCESS_READ, ACCESS_NULL);
        goto error;
    }

    if (status < 0) {
        set_baton_error(error, status, "Failed to modify permissions of '%s' "
                        "to '%s' for '%s'", rods_path->outPath, access_level,
                        owner_specifier);
        goto error;
    }

    logmsg(DEBUG, BATON_CAT, "Set permissions of '%s' to '%s' for '%s'",
           rods_path->outPath, access_level, owner_specifier);

    return status;

error:
    if (conn->rError) {
        logmsg(ERROR, BATON_CAT, error->message);
        log_rods_errstack(ERROR, BATON_CAT, conn->rError);
    }
    else {
        logmsg(ERROR, BATON_CAT, error->message);
    }

    return status;
}

int modify_json_permissions(rcComm_t *conn, rodsPath_t *rods_path,
                            recursive_op recurse, json_t *perm,
                            baton_error_t *error) {
    char *owner_specifier = NULL;
    char *access_level    = NULL;
    init_baton_error(error);

    if (!json_is_object(perm)) {
        set_baton_error(error, -1, "Invalid permissions specifiction: "
                        "not a JSON object");
        goto error;
    }

    json_t *user  = json_object_get(perm, JSON_OWNER_KEY);
    json_t *level = json_object_get(perm, JSON_LEVEL_KEY);
    if (!json_is_string(user)) {
        set_baton_error(error, -1, "Invalid owner: not a JSON string");
        goto error;
    }
    if (!json_is_string(level)) {
        set_baton_error(error, -1, "Invalid access level: not a JSON string");
        goto error;
    }

    owner_specifier = copy_str(json_string_value(user));
    access_level    = copy_str(json_string_value(level));

    int status = modify_permissions(conn, rods_path, recurse, owner_specifier,
                                    access_level, error);

    if (owner_specifier) free(owner_specifier);
    if (access_level)    free(access_level);

    return status;

error:
    logmsg(ERROR, BATON_CAT, error->message);

    if (owner_specifier) free(owner_specifier);
    if (access_level)    free(access_level);

    return -1;
}

int modify_metadata(rcComm_t *conn, rodsPath_t *rods_path,
                    metadata_op operation,
                    char *attr_name, char *attr_value, char *attr_units,
                    baton_error_t *error) {
    int status;
    char *err_name;
    char *err_subname;
    char *type_arg;

    if (rods_path->objState == NOT_EXIST_ST) {
        set_baton_error(error, USER_FILE_DOES_NOT_EXIST,
                        "Path '%s' does not exist "
                        "(or lacks access permission)", rods_path->outPath);
        goto error;
    }

    switch (rods_path->objType) {
        case DATA_OBJ_T:
            logmsg(TRACE, BATON_CAT, "Identified '%s' as a data object",
                   rods_path->outPath);
            type_arg = "-d";
            break;

        case COLL_OBJ_T:
            logmsg(TRACE, BATON_CAT, "Identified '%s' as a collection",
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

    struct mod_metadata_in named_args;
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
        logmsg(ERROR, BATON_CAT, error->message);
        log_rods_errstack(ERROR, BATON_CAT, conn->rError);
    }
    else {
        logmsg(ERROR, BATON_CAT, error->message);
    }

    return status;
}

int modify_json_metadata(rcComm_t *conn, rodsPath_t *rods_path,
                         metadata_op operation, json_t *avu,
                         baton_error_t *error) {
    char *attr_name  = NULL;
    char *attr_value = NULL;
    char *attr_units = NULL;
    init_baton_error(error);

    if (!json_is_object(avu)) {
        set_baton_error(error, -1, "Invalid AVU: not a JSON object");
        goto error;
    }

    json_t *attr  = json_object_get(avu, JSON_ATTRIBUTE_KEY);
    json_t *value = json_object_get(avu, JSON_VALUE_KEY);
    json_t *units = json_object_get(avu, JSON_UNITS_KEY);

    if (!json_is_string(attr)) {
        set_baton_error(error, -1, "Invalid attribute: not a JSON string");
        goto error;
    }
    if (value && !json_is_string(value)) {
        set_baton_error(error, -1, "Invalid value: not a JSON string");
        goto error;
    }
    if (units && !json_is_string(units)) {
        set_baton_error(error, -1, "Invalid units: not a JSON string");
        goto error;
    }

    if (attr)  attr_name  = copy_str(json_string_value(attr));
    if (value) attr_value = copy_str(json_string_value(value));

    // Units are optional
    if (units) {
        attr_units = copy_str(json_string_value(units));
    }
    else {
        attr_units = calloc(1, sizeof (char));
        if (!attr_units) {
            set_baton_error(error, errno,
                            "Failed to allocate memory: error %d %s",
                            errno, strerror(errno));
            goto error;
        }

        attr_units[0] = '\0';
    }

    int status = modify_metadata(conn, rods_path, operation,
                                 attr_name, attr_value, attr_units, error);

    if (attr_name)  free(attr_name);
    if (attr_value) free(attr_value);
    if (attr_units) free(attr_units);

    return status;

error:
    logmsg(ERROR, BATON_CAT, error->message);

    if (attr_name)  free(attr_name);
    if (attr_value) free(attr_value);
    if (attr_units) free(attr_units);

    return -1;
}

genQueryInp_t *make_query_input(int max_rows, int num_columns,
                                const int columns[]) {
    genQueryInp_t *query_in = calloc(1, sizeof (genQueryInp_t));
    if (!query_in) goto error;

    int *cols_to_select = calloc(num_columns, sizeof (int));
    if (!cols_to_select) goto error;

    for (int i = 0; i < num_columns; i++) {
        cols_to_select[i] = columns[i];
    }

    int *special_select_ops = calloc(num_columns, sizeof (int));
    if (!special_select_ops) goto error;

    special_select_ops[0] = 0;

    query_in->selectInp.inx   = cols_to_select;
    query_in->selectInp.value = special_select_ops;
    query_in->selectInp.len   = num_columns;

    query_in->maxRows       = max_rows;
    query_in->continueInx   = 0;
    query_in->condInput.len = 0;

    int *query_cond_indices = calloc(MAX_NUM_CONDITIONALS, sizeof (int));
    if (!query_cond_indices) goto error;

    char **query_cond_values = calloc(MAX_NUM_CONDITIONALS, sizeof (char *));
    if (!query_cond_values) goto error;

    query_in->sqlCondInp.inx   = query_cond_indices;
    query_in->sqlCondInp.value = query_cond_values;
    query_in->sqlCondInp.len   = 0;

    return query_in;

error:
    logmsg(ERROR, BATON_CAT, "Failed to allocate memory: error %d %s",
           errno, strerror(errno));

    return NULL;
}

void free_query_input(genQueryInp_t *query_in) {
    assert(query_in);

    // Free any strings allocated as query clause values
    for (int i = 0; i < query_in->sqlCondInp.len; i++) {
        free(query_in->sqlCondInp.value[i]);
    }

    free(query_in->selectInp.inx);
    free(query_in->selectInp.value);
    free(query_in->sqlCondInp.inx);
    free(query_in->sqlCondInp.value);
    free(query_in);
}

genQueryInp_t *add_query_conds(genQueryInp_t *query_in, int num_conds,
                               const query_cond_t conds[]) {
    for (int i = 0; i < num_conds; i++) {
        const char *operator = conds[i].operator;
        const char *name     = conds[i].value;

        logmsg(DEBUG, BATON_CAT, "Adding conditional %d of %d: %s %s",
               1, num_conds, name, operator);

        int expr_size = strlen(name) + strlen(operator) + 3 + 1;
        char *expr = calloc(expr_size, sizeof (char));
        if (!expr) goto error;

        snprintf(expr, expr_size, "%s '%s'", operator, name);

        logmsg(DEBUG, BATON_CAT,
               "Added conditional %d of %d: %s, len %d, op: %s, "
               "total len %d [%s]",
               i, num_conds, name, strlen(name), operator, expr_size, expr);

        int current_index = query_in->sqlCondInp.len;
        query_in->sqlCondInp.inx[current_index] = conds[i].column;
        query_in->sqlCondInp.value[current_index] = expr;
        query_in->sqlCondInp.len++;
    }

    return query_in;

error:
    logmsg(ERROR, BATON_CAT, "Failed to allocate memory: error %d %s",
           errno, strerror(errno));

    return NULL;
}

json_t *do_query(rcComm_t *conn, genQueryInp_t *query_in,
                 const char *labels[], baton_error_t *error) {
    int status;
    char *err_name;
    char *err_subname;
    genQueryOut_t *query_out = NULL;
    int chunk_num = 0;
    int continue_flag = 0;

    json_t *results = json_array();
    if (!results) {
        set_baton_error(error, -1, "Failed to allocate a new JSON array");
        goto error;
    }

    logmsg(DEBUG, BATON_CAT, "Running query ...");

    while (chunk_num == 0 || continue_flag > 0) {
        logmsg(DEBUG, BATON_CAT, "Attempting to get chunk %d of query",
               chunk_num);

        status = rcGenQuery(conn, query_in, &query_out);

        if (status == 0) {
            logmsg(DEBUG, BATON_CAT, "Successfully fetched chunk %d of query",
                   chunk_num);

            // Cargo-cult from iRODS clients; not sure this is useful
            query_in->continueInx = query_out->continueInx;
            // Allows query_out to be freed
            continue_flag = query_out->continueInx;

            json_t *chunk = make_json_objects(query_out, labels);
            if (!chunk) {
                set_baton_error(error, -1,
                                "Failed to convert query result to JSON: "
                                "in chunk %d error %d", chunk_num, -1);
                goto error;
            }

            logmsg(TRACE, BATON_CAT, "Converted query result to JSON: "
                   "in chunk %d of %d", chunk_num, json_array_size(chunk));
            chunk_num++;

            status = json_array_extend(results, chunk);
            json_decref(chunk);

            if (status != 0) {
                set_baton_error(error, status,
                                "Failed to add JSON query result to total: "
                                "in chunk %d error %d", chunk_num, status);
                goto error;
            }

            // Would be better to somehow realloc this memory
            if (query_out) free(query_out);
        }
        else if (status == CAT_NO_ROWS_FOUND) {
            logmsg(TRACE, BATON_CAT, "Query returned no results");
            break;
        }
        else {
            err_name = rodsErrorName(status, &err_subname);
            set_baton_error(error, status,
                            "Failed to fetch query result: in chunk %d "
                            "error %d %s %s",
                            chunk_num, status, err_name, err_subname);
            goto error;
        }
    }

    logmsg(DEBUG, BATON_CAT, "Obtained a total of %d JSON results in %d chunks",
           chunk_num, json_array_size(results));

    return results;

error:
    if (conn->rError) {
        logmsg(ERROR, BATON_CAT, error->message);
        log_rods_errstack(ERROR, BATON_CAT, conn->rError);
    }
    else {
        logmsg(ERROR, BATON_CAT, error->message);
    }

    if (query_out) free(query_out);
    if (results) json_decref(results);

    return NULL;
}

json_t *make_json_objects(genQueryOut_t *query_out, const char *labels[]) {
    json_t *array = json_array();
    if (!array) {
        logmsg(ERROR, BATON_CAT, "Failed to allocate a new JSON array");
        goto error;
    }

    logmsg(DEBUG, BATON_CAT, "Converting %d rows of results to JSON",
           query_out->rowCnt);

    for (int row = 0; row < query_out->rowCnt; row++) {
        logmsg(DEBUG, BATON_CAT, "Converting row %d of %d to JSON",
               row, query_out->rowCnt);

        json_t *jrow = json_object();
        if (!jrow) {
            logmsg(ERROR, BATON_CAT,"Failed to allocate a new JSON object for "
                   "result row %d of %d", row, query_out->rowCnt);
            goto error;
        }

        for (int i = 0; i < query_out->attriCnt; i++) {
            char *result = query_out->sqlResult[i].value;
            result += row * query_out->sqlResult[i].len;

            logmsg(DEBUG, BATON_CAT,
                   "Encoding column %d '%s' value '%s' as JSON",
                   i, labels[i], result);

            // Skip any results which return as an empty string
            // (notably units, when they are absent from an AVU).
            if (strlen(result) > 0) {
                json_t *jvalue = json_string(result);
                if (!jvalue) {
                    logmsg(ERROR, BATON_CAT,
                           "Failed to parse string '%s'; is it UTF-8?", result);
                    goto error;
                }

                // TODO: check return value
                json_object_set_new(jrow, labels[i], jvalue);
            }
        }

        int status = json_array_append_new(array, jrow);
        if (status != 0) {
            logmsg(ERROR, BATON_CAT,
                   "Failed to append a new JSON result at row %d of %d",
                   row, query_out->rowCnt);
            goto error;
        }
    }

    return array;

error:
    logmsg(ERROR, BATON_CAT, "Failed to convert row %d of %d to JSON");

    if (array) json_decref(array);

    return NULL;
}

json_t *rods_path_to_json(rcComm_t *conn, rodsPath_t *rods_path) {
    json_t *result = NULL;

    switch (rods_path->objType) {
        case DATA_OBJ_T:
            logmsg(TRACE, BATON_CAT, "Identified '%s' as a data object",
                   rods_path->outPath);
            result = data_object_path_to_json(rods_path->outPath);
            break;

        case COLL_OBJ_T:
            logmsg(TRACE, BATON_CAT, "Identified '%s' as a collection",
                   rods_path->outPath);
            result = collection_path_to_json(rods_path->outPath);
            break;

        default:
            logmsg(ERROR, BATON_CAT,
                   "Failed to list metadata on '%s' as it is "
                   "neither data object nor collection",
                   rods_path->outPath);
            goto error;
    }

    if (!result) goto error;

    baton_error_t error;
    json_t *avus = list_metadata(conn, rods_path, NULL, &error);
    if (error.code != 0)  goto error;

    int status = json_object_set_new(result, JSON_AVUS_KEY, avus);
    if (status != 0) goto error;

    return result;

error:
    logmsg(ERROR, BATON_CAT, "Failed to covert '%s' to JSON",
           rods_path->outPath);

    if (result) json_decref(result);

    return NULL;
}

static json_t *list_collection(rcComm_t *conn, rodsPath_t *rods_path,
                               baton_error_t *error) {
    int status;
    char *err_name;
    char *err_subname;

    int query_flags = DATA_QUERY_FIRST_FG;
    collHandle_t coll_handle;
    collEnt_t coll_entry;

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

    json_t *results = json_array();
    if (!results) {
        set_baton_error(error, -1, "Failed to allocate a new JSON array");
        goto error;
    }

    while ((status = rclReadCollection(conn, &coll_handle, &coll_entry)) >= 0) {
        json_t *entry;

        switch (coll_entry.objType) {
            case DATA_OBJ_T:
                logmsg(TRACE, BATON_CAT, "Identified '%s/%s' as a data object",
                       coll_entry.collName, coll_entry.dataName);
                entry = data_object_parts_to_json(coll_entry.collName,
                                                  coll_entry.dataName);
                if (!entry) {
                    set_baton_error(error, -1, "Failed to pack '%s/%s' as JSON",
                                    coll_entry.collName, coll_entry.dataName);
                    goto query_error;
                }
                break;

            case COLL_OBJ_T:
                logmsg(TRACE, BATON_CAT, "Identified '%s' as a collection",
                       coll_entry.collName);
                entry = collection_path_to_json(coll_entry.collName);
                if (!entry) {
                    set_baton_error(error, -1, "Failed to pack '%s' as JSON",
                                    coll_entry.collName);
                    goto query_error;
                }
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
        logmsg(ERROR, BATON_CAT, error->message);
        log_rods_errstack(ERROR, BATON_CAT, conn->rError);
    }
    else {
        logmsg(ERROR, BATON_CAT, error->message);
    }

    return NULL;
}

static void map_mod_args(modAVUMetadataInp_t *out, struct mod_metadata_in *in) {
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

static genQueryInp_t *prepare_obj_list(genQueryInp_t *query_in,
                                       rodsPath_t *rods_path,
                                       const char *attr_name) {
    char *path = rods_path->outPath;
    size_t len = strlen(path) + 1;

    char *path1 = calloc(len, sizeof (char));
    char *path2 = calloc(len, sizeof (char));
    if (!path1) goto error;
    if (!path2) goto error;

    strncpy(path1, path, len);
    strncpy(path2, path, len);

    char *coll_name = dirname(path1);
    char *data_name = basename(path2);

    query_cond_t cn = { .column   = COL_COLL_NAME,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = coll_name };
    query_cond_t dn = { .column   = COL_DATA_NAME,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = data_name };
    query_cond_t an = { .column   = COL_META_DATA_ATTR_NAME,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = attr_name };

    int num_conds = 2;
    if (attr_name) {
        add_query_conds(query_in, num_conds + 1,
                        (query_cond_t []) { cn, dn, an });
    }
    else {
        add_query_conds(query_in, num_conds, (query_cond_t []) { cn, dn });
    }

    free(path1);
    free(path2);

    return query_in;

error:
    logmsg(ERROR, BATON_CAT, "Failed to allocate memory: error %d %s",
           errno, strerror(errno));

    return NULL;
}

static json_t *add_acl_json_array(rcComm_t *conn, json_t *array,
                                  baton_error_t *error) {
    if (!json_is_array(array)) {
        set_baton_error(error, -1, "Invalid target: not a JSON array");
        goto error;
    }

    for (size_t i = 0; i < json_array_size(array); i++) {
        json_t *item = json_array_get(array, i);
        add_acl_json_object(conn, item, error);
        if (error->code != 0) goto error;
    }

    return array;

error:
    logmsg(ERROR, BATON_CAT, error->message);

    return NULL;
}

static json_t *add_acl_json_object(rcComm_t *conn, json_t *object,
                                   baton_error_t *error) {
    rodsPath_t rods_path;
    char *path;

    if (!json_is_object(object)) {
        set_baton_error(error, -1, "Invalid target: not a JSON object");
        goto error;
    }

    path = json_to_path(object, error);
    if (error->code != 0) goto error;

    int status = set_rods_path(conn, &rods_path, path);
    if (status < 0) {
        set_baton_error(error, status, "Failed to set iRODS path '%s'", path);
        goto error;
    }

    json_t *perms = list_permissions(conn, &rods_path, error);
    if (error->code != 0) goto error;

    add_permissions(object, perms, error);
    if (error->code != 0) goto error;

    if (path) free(path);
    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);

    return object;

error:
    logmsg(ERROR, BATON_CAT, error->message);

    if (path) free(path);
    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);

    return NULL;
}

static genQueryInp_t *prepare_obj_acl_list(genQueryInp_t *query_in,
                                           rodsPath_t *rods_path) {
    char *data_id = rods_path->dataId;
    query_cond_t di = { .column   = COL_DATA_ACCESS_DATA_ID,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = data_id };
    query_cond_t tn = { .column   = COL_DATA_TOKEN_NAMESPACE,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = ACCESS_NAMESPACE };

    int num_conds = 2;
    return add_query_conds(query_in, num_conds, (query_cond_t []) { di, tn });
}

static genQueryInp_t *prepare_col_acl_list(genQueryInp_t *query_in,
                                           rodsPath_t *rods_path) {
    char *path = rods_path->outPath;
    query_cond_t cn = { .column   = COL_COLL_NAME,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = path };
    query_cond_t tn = { .column   = COL_COLL_TOKEN_NAMESPACE,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = ACCESS_NAMESPACE };
    int num_conds = 2;
    return add_query_conds(query_in, num_conds, (query_cond_t []) { cn, tn });
}

static genQueryInp_t *prepare_obj_acl_search(genQueryInp_t *query_in,
                                             rodsPath_t *rods_path,
                                             const char *user_id,
                                             const char *perm_id) {
    char *data_id = rods_path->dataId;
    query_cond_t di = { .column   = COL_DATA_ACCESS_DATA_ID,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = data_id };
    query_cond_t tn = { .column   = COL_DATA_TOKEN_NAMESPACE,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = ACCESS_NAMESPACE };
    query_cond_t ui = { .column   = COL_DATA_ACCESS_USER_ID,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = user_id };
    query_cond_t pm = { .column   = COL_DATA_ACCESS_TYPE,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = perm_id };
    int num_conds = 4;
    return add_query_conds(query_in, num_conds,
                           (query_cond_t []) { di, tn, ui, pm });
}

static genQueryInp_t *prepare_col_list(genQueryInp_t *query_in,
                                       rodsPath_t *rods_path,
                                       const char *attr_name) {
    char *path = rods_path->outPath;
    query_cond_t cn = { .column   = COL_COLL_NAME,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = path };
    query_cond_t an = { .column   = COL_META_COLL_ATTR_NAME,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = attr_name };

    int num_conds = 1;
    if (attr_name) {
        add_query_conds(query_in, num_conds + 1, (query_cond_t []) { cn, an });
    }
    else {
        add_query_conds(query_in, num_conds, (query_cond_t []) { cn });
    }

    return query_in;
}

static genQueryInp_t *prepare_obj_search(genQueryInp_t *query_in,
                                         const char *attr_name,
                                         const char *attr_value,
                                         const char *operator) {
    query_cond_t an = { .column   = COL_META_DATA_ATTR_NAME,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = attr_name };
    query_cond_t av = { .column   = COL_META_DATA_ATTR_VALUE,
                        .operator = operator,
                        .value    = attr_value };
    int num_conds = 2;
    return add_query_conds(query_in, num_conds, (query_cond_t []) { an, av });
}

static genQueryInp_t *prepare_col_search(genQueryInp_t *query_in,
                                         const char *attr_name,
                                         const char *attr_value,
                                         const char *operator) {
    query_cond_t an = { .column   = COL_META_COLL_ATTR_NAME,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = attr_name };
    query_cond_t av = { .column   = COL_META_COLL_ATTR_VALUE,
                        .operator = operator,
                        .value    = attr_value };
    int num_conds = 2;
    return add_query_conds(query_in, num_conds, (query_cond_t []) { an, av });
}

static genQueryInp_t *prepare_path_search(genQueryInp_t *query_in,
                                          const char *root_path) {
    size_t len = strlen(root_path);
    char *path;

    if (len > 0) {
        // Absolute path
        if (str_starts_with(root_path, "/")) {
            path = calloc(len + 2, sizeof (char));
            if (!path) goto error;

            snprintf(path, len + 2, "%s%%", root_path);
        }
        else {
            path = calloc(len + 3, sizeof (char));
            if (!path) goto error;

            snprintf(path, len + 3, "%%%s%%", root_path);
        }

        query_cond_t pv = { .column   = COL_COLL_NAME,
                            .operator = SEARCH_OP_LIKE,
                            .value    = path };

        int num_conds = 1;
        add_query_conds(query_in, num_conds, (query_cond_t []) { pv });
        free(path);
    }

    return query_in;

error:
    logmsg(ERROR, BATON_CAT, "Failed to allocate memory: error %d %s",
           errno, strerror(errno));

    return NULL;
}

static genQueryInp_t *prepare_user_search(genQueryInp_t *query_in,
                                          const char *user_name) {
    query_cond_t un = { .column   = COL_USER_NAME,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = user_name };

    int num_conds = 1;
    return add_query_conds(query_in, num_conds,  (query_cond_t []) { un });
}
