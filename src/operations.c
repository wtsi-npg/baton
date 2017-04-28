/**
 * Copyright (C) 2017 Genome Research Ltd. All rights reserved.
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
 * @file operation.c
 * @author Keith James <kdj@sanger.ac.uk>
 */

#include "config.h"

#include "baton.h"
#include "operations.h"

static int iterate_json(FILE *input, rodsEnv *env, rcComm_t *conn,
                        baton_json_op fn, option_flags flags, int *item_count,
                        va_list args) {
    int error_count = 0;

    while (!feof(input)) {
        size_t jflags = JSON_DISABLE_EOF_CHECK | JSON_REJECT_DUPLICATES;
        json_error_t load_error;
        json_t *doc = json_loadf(input, jflags, &load_error);

        if (!doc) {
            if (!feof(input)) {
                logmsg(ERROR, "JSON error at line %d, column %d: %s",
                       load_error.line, load_error.column, load_error.text);
            }

            continue;
        }

        if (!json_is_object(doc)) {
            logmsg(ERROR, "Item %d in stream was not a JSON object; skipping",
                   item_count);
            error_count++;
            json_decref(doc);
            continue;
        }

        va_list fnargs;
        va_copy(fnargs, args);

        baton_error_t error;
        json_t *result = fn(env, conn, doc, flags, &error, fnargs);
        if (error.code != 0) {
            error_count++;
            add_error_value(doc, &error);
        }
        else if (has_operation(doc) && has_operation_target(doc)) {
            // It's an envelope, so we add the result as a property
            add_result(doc, result, &error);
            if (error.code != 0) {
                logmsg(ERROR, "Failed to add error report to item %d "
                       "in stream. Error code %d: %s", item_count,
                       error.code, error.message);
                error_count++;
            }
            print_json(doc);
        }
        else {
            // There is no envelope, so the result is reported
            // directly
            print_json(result);
            json_decref(result);
        }
        if (flags & FLUSH) fflush(stdout);

        va_end(fnargs);
        (*item_count)++;

        json_decref(doc);
    } // while

    return error_count;
}

int do_operation(FILE *input, baton_json_op fn, option_flags flags, ...) {
    int item_count  = 0;
    int error_count = 0;

    rodsEnv env;
    rcComm_t *conn = rods_login(&env);
    if (!conn) goto error;

    va_list args;
    va_start(args, flags);

    error_count =
        iterate_json(input, &env, conn, fn, flags, &item_count, args);
    if (error_count > 0) {
        logmsg(WARN, "Processed %d items with %d errors",
               item_count, error_count);
    }
    else {
        logmsg(DEBUG, "Processed %d items with %d errors",
               item_count, error_count);
    }

    va_end(args);

    rcDisconnect(conn);

    return error_count;

error:
    if (conn) rcDisconnect(conn);

    logmsg(ERROR, "Processed %d items with %d errors",
           item_count, error_count);

    return 1;
}

json_t *baton_json_dispatch_op(rodsEnv *env, rcComm_t *conn, json_t *envelope,
                               option_flags flags, baton_error_t *error,
                               va_list args) {
    json_t *result = NULL;

    const char *op = get_operation_name(envelope, error);
    if (error->code != 0) {
        result = error_to_json(error);
        goto error;
    }

    json_t *target = get_operation_target(envelope, error);
    if (error->code != 0) {
        result = error_to_json(error);
        goto error;
    }

    if (has_operation_params(envelope)) {
        json_t *params = get_operation_params(envelope, error);
        if (error->code != 0) {
            result = error_to_json(error);
            goto error;
        }

        if (acl_p(params))        flags = flags | PRINT_ACL;
        if (avu_p(params))        flags = flags | PRINT_AVU;
        if (checksum_p(params))   flags = flags | PRINT_CHECKSUM;
        if (contents_p(params))   flags = flags | PRINT_CONTENTS;
        if (replicate_p(params))  flags = flags | PRINT_REPLICATE;
        if (timestamp_p(params))  flags = flags | PRINT_TIMESTAMP;
        if (collection_p(params)) flags = flags | SEARCH_COLLECTIONS;
        if (object_p(params))     flags = flags | SEARCH_OBJECTS;
    }

    if (str_equals(op, JSON_CHMOD_OPERATION, MAX_STR_LEN)) {
        logmsg(DEBUG, "Dispatching to operation '%s'", op);
        result = baton_json_chmod_op(env, conn, target, flags, error, args);
    }
    else if (str_equals(op, JSON_LIST_OPERATION, MAX_STR_LEN)) {
        logmsg(DEBUG, "Dispatching to operation '%s'", op);
        result = baton_json_list_op(env, conn, target, flags, error, args);
    }
    else if (str_equals(op, JSON_METAMOD_OPERATION, MAX_STR_LEN)) {
        logmsg(DEBUG, "Dispatching to operation '%s'", op);
        result = baton_json_metamod_op(env, conn, target, flags, error, args);
    }
    else if (str_equals(op, JSON_METAQUERY_OPERATION, MAX_STR_LEN)) {
        logmsg(DEBUG, "Dispatching to operation '%s'", op);
        result = baton_json_metaquery_op(env, conn, target, flags, error, args);
    }
    else if (str_equals(op, JSON_GET_OPERATION, MAX_STR_LEN)) {
        logmsg(DEBUG, "Dispatching to operation '%s'", op);
        result = baton_json_get_op(env, conn, target, flags, error, args);
    }
    else {
        set_baton_error(error, -1, "Invalid baton operation '%s'", op);
        result = error_to_json(error);
        goto error;
    }

    return result;

error:
    return result;
}

json_t *baton_json_list_op(rodsEnv *env, rcComm_t *conn, json_t *target,
                           option_flags flags, baton_error_t *error,
                           va_list args) {
    (void) args; // Not used

    json_t *result = NULL;

    char *path = json_to_path(target, error);
    if (error->code != 0) {
        result = error_to_json(error);
        goto error;
    }

    rodsPath_t rods_path;
    resolve_rods_path(conn, env, &rods_path, path, flags, error);
    if (error->code != 0) {
        result = error_to_json(error);
        goto error;
    }

    result = list_path(conn, &rods_path, flags, error);
    if (error->code != 0) {
        result = error_to_json(error);
        goto error;
    }

    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);
    if (path) free(path);

    return result;

error:
    if (path) free(path);

    return result;
}

json_t *baton_json_chmod_op(rodsEnv *env, rcComm_t *conn, json_t *target,
                            option_flags flags, baton_error_t *error,
                            va_list args) {
    (void) args; // Not used

    json_t *result = NULL;

    char *path = json_to_path(target, error);
    if (error->code != 0) {
        result = error_to_json(error);
        goto error;
    }

    json_t *perms = json_object_get(target, JSON_ACCESS_KEY);
    if (!json_is_array(perms)) {
        set_baton_error(error, -1, "Permissions data for %s is not in "
                        "a JSON array", path);
        result = error_to_json(error);
        goto error;
    }

    rodsPath_t rods_path;
    resolve_rods_path(conn, env, &rods_path, path, flags, error);
    if (error->code != 0) {
        result = error_to_json(error);
        goto error;
    }

    recursive_op recurse = (flags & RECURSIVE) ? RECURSE : NO_RECURSE;

    for (size_t i = 0; i < json_array_size(perms); i++) {
        json_t *perm = json_array_get(perms, i);
        modify_json_permissions(conn, &rods_path, recurse, perm, error);

        if (error->code != 0) {
            result = error_to_json(error);
            goto error;
        }
    }

    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);
    if (path) free(path);

    return result;

error:
    if (path) free(path);

    return result;
}

json_t *baton_json_metaquery_op(rodsEnv *env, rcComm_t *conn, json_t *target,
                                option_flags flags, baton_error_t *error,
                                va_list args) {
    json_t *result = NULL;

    if (has_collection(target)) {
        resolve_collection(target, conn, env, flags, error);
        if (error->code != 0) {
            result = error_to_json(error);
            goto error;
        }
    }

    char *zone_name = va_arg(args, char *);
    logmsg(DEBUG, "Metadata query in zone '%s'", zone_name);

    result = search_metadata(conn, target, zone_name, flags, error);
    if (error->code != 0) {
        result = error_to_json(error);
        goto error;
    }

    return result;

error:
    return result;
}

json_t *baton_json_metamod_op(rodsEnv *env, rcComm_t *conn, json_t *target,
                              option_flags flags, baton_error_t *error,
                              va_list args) {
    (void) args; // Not used

    json_t *result = NULL;

    char *path = json_to_path(target, error);
    if (error->code != 0) {
        result = error_to_json(error);
        goto error;
    }

    json_t *avus = json_object_get(target, JSON_AVUS_KEY);
    if (!json_is_array(avus)) {
        set_baton_error(error, -1, "AVU data for %s is not in a JSON array",
                        path);
        result = error_to_json(error);
        goto error;
    }

    rodsPath_t rods_path;
    resolve_rods_path(conn, env, &rods_path, path, flags, error);
    if (error->code != 0) {
        result = error_to_json(error);
        goto error;
    }

    metadata_op operation;
    if (flags & ADD_AVU) {
        operation = META_ADD;
    }
    else if (flags & REMOVE_AVU) {
        operation = META_REM;
    }
    else {
        set_baton_error(error, -1, "No metadata operation was requested ",
                        " for %s", path);
        result = error_to_json(error);
        goto error;
    }

    for (size_t i = 0; i < json_array_size(avus); i++) {
        json_t *avu = json_array_get(avus, i);
        modify_json_metadata(conn, &rods_path, operation, avu, error);
        if (error->code != 0) {
            result = error_to_json(error);
            goto error;
        }
    }

    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);

    return result;

error:
    if (path) free(path);

    return result;
}

json_t *baton_json_get_op(rodsEnv *env, rcComm_t *conn, json_t *target,
                          option_flags flags, baton_error_t *error,
                          va_list args) {
    json_t *result = NULL;

    char *path = json_to_path(target, error);
    if (error->code != 0) {
        result = error_to_json(error);
        goto error;
    }

    rodsPath_t rods_path;
    resolve_rods_path(conn, env, &rods_path, path, flags, error);
    if (error->code != 0) {
        result = error_to_json(error);
        goto error;
    }

    va_arg(args, char *); // zone_name not used
    size_t buffer_size = va_arg(args, size_t);

    logmsg(DEBUG, "Using a 'get' buffer size of %zu bytes", buffer_size);

    if (flags & SAVE_FILES) {
        char *file = NULL;
        file = json_to_local_path(target, error);
        if (file) {
            write_path_to_file(conn, &rods_path, file, buffer_size, error);
            free(file);
        }

        if (error->code != 0) {
            result = error_to_json(error);
            goto error;
        }
    }
    else if (flags & PRINT_RAW) {
        write_path_to_stream(conn, &rods_path, stdout, buffer_size, error);
        if (error->code != 0) {
            result = error_to_json(error);
            goto error;
        }
    }
    else {
        result = ingest_path(conn, &rods_path, flags, buffer_size, error);
        if (error->code != 0) {
            result = error_to_json(error);
            goto error;
        }
    }

    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);

    return result;

error:
    if (path) free(path);

    return result;
}

int check_str_arg(const char *arg_name, const char *arg_value,
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
