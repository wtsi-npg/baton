/**
 * Copyright (C) 2013, 2014, 2015, 2016, 2017, 2020 Genome Research
 * Ltd. All rights reserved.
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
 * @author Joshua C. Randall <jcrandall@alum.mit.edu>
 */

#include <errno.h>
#include <libgen.h>
#include <math.h>
#include <regex.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "baton.h"
#include "signal_handler.h"

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

static rcComm_t *rods_connect(rodsEnv *env){
    rcComm_t *conn = NULL;
    rErrMsg_t errmsg;

    // TODO: add option for NO_RECONN vs. RECONN_TIMEOUT
    conn = rcConnect(env->rodsHost, env->rodsPort, env->rodsUserName,
                     env->rodsZone, NO_RECONN, &errmsg);

    if (!conn) goto finally;

    int sigstatus = apply_signal_handler();
    if (sigstatus != 0) {
        exit(1);
    }

finally:
    return conn;
}

int is_irods_available() {
    rcComm_t *conn = NULL;
    rodsEnv env;
    int status;

    status = getRodsEnv(&env);
    if (status < 0) {
        logmsg(ERROR, "Failed to load your iRODS environment");
        goto error;
    }

    conn = rods_connect(&env);

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

char* get_client_version() {
    int ver [3] = { IRODS_VERSION_MAJOR,
                    IRODS_VERSION_MINOR,
                    IRODS_VERSION_PATCHLEVEL };

    int total_chars = 0;
    for (int i = 0; i < 3; i++) {
        int num_digits = (ver[i] == 0) ? 1 : log10(ver[i]) + 1;
        total_chars += num_digits;
    }

    total_chars += 2; // Two dots

    char *version = calloc(total_chars + 1, sizeof (char));
    snprintf(version, total_chars + 1, "%d.%d.%d", ver[0], ver[1], ver[2]);

    return version;
}

char* get_server_version(rcComm_t *conn, baton_error_t *error) {
    const char *ver_re_str = "([0-9]+\\.[0-9]+\\.[0-9]+)$";
    int ver_re_idx = 0;
    regex_t ver_re;
    regmatch_t ver_match[ver_re_idx + 1];

    int restatus;
    char remsg[MAX_ERROR_MESSAGE_LEN];

    miscSvrInfo_t *server_info;
    char *version = NULL;

    init_baton_error(error);

    restatus = regcomp(&ver_re, ver_re_str, REG_EXTENDED | REG_ICASE);
    if (restatus != 0) {
        regerror(restatus, &ver_re, remsg, MAX_ERROR_MESSAGE_LEN);
        set_baton_error(error, restatus, "Could not compile regex: '%s': %s",
                        ver_re_str, remsg);
        goto error;
    }

    int status = rcGetMiscSvrInfo(conn, &server_info);
    if (status < 0) {
        char *err_subname;
        const char *err_name = rodsErrorName(status, &err_subname);
        set_baton_error(error, status, "Failed get server information: %d %s",
                        status, err_name);
        goto error;
    }

    const char *ver = server_info->relVersion;
    restatus = regexec(&ver_re, ver, 1, ver_match, 0);
    if (!restatus) {
        int start = ver_match[ver_re_idx].rm_so;
        int   end = ver_match[ver_re_idx].rm_eo;
        int len   = end + 1 - start;
        version = calloc(len, sizeof (char));
        strncpy(version, ver + start, len);
    }
    else if (restatus == REG_NOMATCH) {
        set_baton_error(error, restatus, "Failed to match server version: '%s'",
                        ver);
        goto error;
    }
    else {
        regerror(restatus, &ver_re, remsg, MAX_ERROR_MESSAGE_LEN);
        set_baton_error(error, restatus,
                        "Failed to match server version: '%s': %s", ver, remsg);
        goto error;
    }

    return version;

error:
    regfree(&ver_re);
    if (version) free(version);
    return NULL;
}

rcComm_t *rods_login(rodsEnv *env) {
    rcComm_t *conn = NULL;
    int status;

    status = getRodsEnv(env);
    if (status < 0) {
        logmsg(ERROR, "Failed to load your iRODS environment");
        goto error;
    }

    conn = rods_connect(env);
    if (!conn) {
        logmsg(ERROR, "Failed to connect to %s:%d zone '%s' as '%s'",
               env->rodsHost, env->rodsPort, env->rodsZone, env->rodsUserName);
        goto error;
    }

#if IRODS_VERSION_INTEGER && IRODS_VERSION_INTEGER >= 4001008
#if IRODS_VERSION_INTEGER >= (4*1000000 + 2*1000 + 8)
    load_client_api_plugins();
#else
    init_client_api_table();
#endif
    status = clientLogin(conn, "", "");
#else
    status = clientLogin(conn);
#endif

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

    char *dest = rstrcpy(rods_path->inPath, inpath, MAX_NAME_LEN);
    if (!dest) return USER_PATH_EXCEEDS_MAX;

    rods_path->objType  = UNKNOWN_OBJ_T;
    rods_path->objState = UNKNOWN_ST;

    return 0;
}

int resolve_rods_path(rcComm_t *conn, rodsEnv *env, rodsPath_t *rods_path,
                      char *inpath, option_flags flags, baton_error_t *error) {
    init_baton_error(error);

    if (!str_starts_with(inpath, "/", 1)) {
        const char *message = "Found relative collection path '%s'. "
            "Using relative collection paths in iRODS may be "
            "dangerous because the CWD may change unexpectedly. "
            "See https://github.com/irods/irods/issues/2406";

        if (flags & UNSAFE_RESOLVE) {
            logmsg(WARN, message, inpath);
        }
        else {
            set_baton_error(error, -1, message, inpath);
            goto error;
        }
    }

    int status = init_rods_path(rods_path, inpath);
    if (status < 0) {
        set_baton_error(error, status,
                        "Failed to create iRODS path '%s'", inpath);
        goto error;
    }

    status = parseRodsPath(rods_path, env);
    if (status < 0) {
        set_baton_error(error, status, "Failed to parse path '%s'",
                        rods_path->inPath);
        goto error;
    }

    status = getRodsObjType(conn, rods_path);
    if (status < 0) {
        char *err_subname;
        const char *err_name = rodsErrorName(status, &err_subname);
        set_baton_error(error, status,
                        "Failed to get the type of iRODS path '%s': %d %s",
                        rods_path->inPath, status, err_name);
        goto error;
    }

    return status;

error:
    return error->code;
}

int set_rods_path(rcComm_t *conn, rodsPath_t *rods_path, char *path,
                  baton_error_t *error) {
    char *dest;

    int status = init_rods_path(rods_path, path);
    if (status < 0) {
        set_baton_error(error, status,
                        "Failed to create iRODS path '%s'", path);
        goto error;
    }

    dest = rstrcpy(rods_path->outPath, path, MAX_NAME_LEN);
    if (!dest) {
        set_baton_error(error, USER_PATH_EXCEEDS_MAX,
                        "iRODS path '%s' is too long (exceeds %d",
                        path, MAX_NAME_LEN);
        goto error;
    }

    status = getRodsObjType(conn, rods_path);
    if (status < 0) {
        char *err_subname;
        const char *err_name = rodsErrorName(status, &err_subname);
        set_baton_error(error, status,
                        "Failed to get the type of iRODS path '%s': %d %s",
                        rods_path->inPath, status, err_name);
        goto error;
    }

    if (status != EXIST_ST) {
        set_baton_error(error, status,
                        "iRODS path does not exist '%s'", path);
        goto error;
    }

    return status;

error:
    return error->code;
}

int move_rods_path(rcComm_t *conn, rodsPath_t *rods_path, char *new_path,
                   baton_error_t *error) {
    dataObjCopyInp_t obj_rename_in;
    int status;

    init_baton_error(error);

    memset(&obj_rename_in, 0, sizeof (dataObjCopyInp_t));

    fprintf(stderr, "MOVING %s to %s\n", rods_path->outPath, new_path);

    switch (rods_path->objType) {
        case DATA_OBJ_T:
            logmsg(TRACE, "Identified '%s' as a data object",
                   rods_path->outPath);
            obj_rename_in.destDataObjInp.oprType = RENAME_DATA_OBJ;
        break;

        case COLL_OBJ_T:
            logmsg(TRACE, "Identified '%s' as a collection",
                   rods_path->outPath);
            obj_rename_in.destDataObjInp.oprType = RENAME_COLL;
            break;

        default:
            set_baton_error(error, USER_INPUT_PATH_ERR,
                            "Failed to move '%s' as it is "
                            "neither data object nor collection",
                            rods_path->outPath);
            goto finally;
    }

    check_str_arg("path", new_path, MAX_NAME_LEN, error);
    if (error->code != 0) goto finally;

    char *src = rstrcpy(obj_rename_in.srcDataObjInp.objPath,
                        rods_path->outPath, MAX_NAME_LEN);
    if (!src) {
        set_baton_error(error, USER_PATH_EXCEEDS_MAX,
                        "iRODS source path '%s' is too long (exceeds %d",
                        rods_path->outPath, MAX_NAME_LEN);
        goto finally;
    }

    char *dest = rstrcpy(obj_rename_in.destDataObjInp.objPath,
                         new_path, MAX_NAME_LEN);
    if (!dest) {
        set_baton_error(error, USER_PATH_EXCEEDS_MAX,
                        "iRODS destination path '%s' is too long (exceeds %d",
                        rods_path->outPath, MAX_NAME_LEN);
        goto finally;
    }

    status = rcDataObjRename(conn, &obj_rename_in);
    if (status < 0) {
        char *err_subname;
        const char *err_name = rodsErrorName(status, &err_subname);
        set_baton_error(error, status,
                        "Failed to rename '%s' to '%s': %d %s",
                        src, dest, status, err_name);
    }

finally:
    return error->code;
}

int resolve_collection(json_t *object, rcComm_t *conn, rodsEnv *env,
                       option_flags flags, baton_error_t *error) {
    char *collection = NULL;

    init_baton_error(error);

    if (!json_is_object(object)) {
        set_baton_error(error, -1, "Failed to resolve the iRODS collection: "
                        "target not a JSON object");
        goto finally;
    }
    if (!has_collection(object)) {
        set_baton_error(error, -1, "Failed to resolve the iRODS collection: "
                        "target has no collection property");
        goto finally;
    }

    rodsPath_t rods_path;
    const char *unresolved = get_collection_value(object, error);
    if (error->code != 0) goto finally;

    logmsg(DEBUG, "Attempting to resolve collection '%s'", unresolved);

    collection = json_to_collection_path(object, error);
    if (error->code != 0) goto finally;

    resolve_rods_path(conn, env, &rods_path, collection, flags, error);
    if (error->code != 0) goto finally;

    logmsg(DEBUG, "Resolved collection '%s' to '%s'", unresolved,
           rods_path.outPath);

    json_object_del(object, JSON_COLLECTION_KEY);
    json_object_del(object, JSON_COLLECTION_SHORT_KEY);

    add_collection(object, rods_path.outPath, error);
    if (error->code != 0) goto finally;

    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);

finally:
    if (collection) free(collection);

    return error->code;
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
              .good_repl   = 1 };
    }
    else {
        obj_format = &(query_format_in_t)
            { .num_columns = 2,
              .columns     = { COL_COLL_NAME, COL_DATA_NAME },
              .labels      = { JSON_COLLECTION_KEY, JSON_DATA_OBJECT_KEY },
              .good_repl   = 0 };
    }

    init_baton_error(error);

    if (zone_name) {
        check_str_arg("zone_name", zone_name, NAME_LEN, error);
        if (error->code != 0) goto error;
    }

    query = map_access_args(query, error);
    if (error->code != 0) goto error;

    results = json_array();
    if (!results) {
        set_baton_error(error, -1, "Failed to allocate a new JSON array");
        goto error;
    }

    if (flags & SEARCH_COLLECTIONS) {
        logmsg(DEBUG, "Searching for collections ...");
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
        logmsg(DEBUG, "Searching for data objects ...");
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
    if (flags & PRINT_CHECKSUM) {
        results = add_checksum_json_array(conn, results, error);
        if (error->code != 0) goto error;
    }
    if (flags & PRINT_TIMESTAMP) {
        results = add_tps_json_array(conn, results, error);
        if (error->code != 0) goto error;
    }
    if (flags & PRINT_REPLICATE) {
        results = add_repl_json_array(conn, results, error);
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

json_t *search_specific(rcComm_t *conn, json_t *query, char *zone_name,
                        baton_error_t *error) {
    json_t *results = NULL;

    init_baton_error(error);

    if (zone_name) {
        check_str_arg("zone_name", zone_name, NAME_LEN, error);
        if (error->code != 0) goto error;
    }

    logmsg(TRACE, "Running specific query ...");
    results = do_specific(conn, zone_name, query, prepare_specific_query,
                          prepare_specific_labels, error);

    if (error->code != 0) goto error;

    return results;

error:
    logmsg(ERROR, error->message);

    if (results) json_decref(results);

    return NULL;
}

int modify_permissions(rcComm_t *conn, rodsPath_t *rods_path,
                       recursive_op recurse, char *owner_specifier,
                       char *access_level, baton_error_t *error) {
    char user_name[NAME_LEN];
    char zone_name[NAME_LEN];
    modAccessControlInp_t mod_perms_in;
    int status;

    init_baton_error(error);

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
    const char *zone = NULL;
    const char *level;

    init_baton_error(error);

    zone = get_access_zone(access, error);
    owner = get_access_owner(access, error);
    if (error->code != 0) goto finally;

    if (zone) {
        snprintf(owner_specifier, sizeof owner_specifier, "%s#%s",
                 owner, zone);
    }
    else {
        snprintf(owner_specifier, sizeof owner_specifier, "%s", owner);
    }

    level = get_access_level(access, error);
    snprintf(access_level, sizeof access_level, "%s", level);
    if (error->code != 0) goto finally;

    modify_permissions(conn, rods_path, recurse, owner_specifier,
                       access_level, error);

finally:
    return error->code;
}

int modify_metadata(rcComm_t *conn, rodsPath_t *rods_path,
                    metadata_op operation,
                    char *attr_name, char *attr_value, char *attr_units,
                    baton_error_t *error) {
    char *type_arg;

    init_baton_error(error);

    check_str_arg("attr_name", attr_name, MAX_STR_LEN, error);
    if (error->code != 0) goto error;

    check_str_arg("attr_value", attr_value, MAX_STR_LEN, error);
    if (error->code != 0) goto error;

    // attr_units may be empty but should not be NULL
    check_str_arg_permit_empty("attr_units", attr_units, MAX_STR_LEN, error);
    if (error->code != 0) goto error;

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

    int status = rcModAVUMetadata(conn, &anon_args);
    if (status < 0) {
        char *err_subname;
        const char *err_name = rodsErrorName(status, &err_subname);
        set_baton_error(error, status,
                        "Failed to %s metadata '%s' -> '%s' on '%s': "
                        "error %d %s", metadata_op_name(operation),
                        attr_name, attr_value, rods_path->outPath,
                        status, err_name);
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

int maybe_modify_json_metadata(rcComm_t *conn, rodsPath_t *rods_path,
                               metadata_op operation,
                               json_t *candidate_avus, json_t *reference_avus,
                               baton_error_t *error) {
    const char *op_name = metadata_op_name(operation);

    init_baton_error(error);

    for (size_t i = 0; i < json_array_size(candidate_avus); i++) {
        json_t *candidate_avu = json_array_get(candidate_avus, i);
        char *str = json_dumps(candidate_avu, JSON_DECODE_ANY);

        if (contains_avu(reference_avus, candidate_avu)) {
            logmsg(TRACE, "Skipping '%s' operation on AVU %s", op_name, str);
        }
        else {
            logmsg(TRACE, "Performing '%s' operation on AVU %s", op_name, str);
            modify_json_metadata(conn, rods_path, operation, candidate_avu,
                                 error);
        }

        free(str);

        if (error->code != 0) goto finally;
    }

finally:
    return error->code;
}

int modify_json_metadata(rcComm_t *conn, rodsPath_t *rods_path,
                         metadata_op operation, json_t *avu,
                         baton_error_t *error) {
    char *attr_tmp  = NULL;
    char *value_tmp = NULL;
    char *units_tmp = NULL;

    init_baton_error(error);

    const char *attr = get_avu_attribute(avu, error);
    if (error->code != 0) goto finally;

    const char *value = get_avu_value(avu, error);
    if (error->code != 0) goto finally;

    const char *units = get_avu_units(avu, error);
    if (error->code != 0) goto finally;

    attr_tmp = copy_str(attr, MAX_STR_LEN);
    if (!attr_tmp) {
        set_baton_error(error, errno,
                        "Failed to allocate memory for attribute");
        goto finally;
    }

    value_tmp = copy_str(value, MAX_STR_LEN);
    if (!value_tmp) {
        set_baton_error(error, errno, "Failed to allocate memory value");
        goto finally;
    }

    // Units are optional
    if (!units) { units = ""; }
    units_tmp = copy_str(units, MAX_STR_LEN);
    if (!units_tmp) {
        set_baton_error(error, errno,
                        "Failed to allocate memory for units");
        goto finally;
    }

    modify_metadata(conn, rods_path, operation,
                    attr_tmp, value_tmp, units_tmp, error);

finally:
    if (attr_tmp)  free(attr_tmp);
    if (value_tmp) free(value_tmp);
    if (units_tmp) free(units_tmp);

    return error->code;
}
