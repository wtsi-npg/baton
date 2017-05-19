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
                        baton_json_op fn, operation_args_t *args,
                        int *item_count) {
    int error_count = 0;

    while (!feof(input)) {
        size_t jflags = JSON_DISABLE_EOF_CHECK | JSON_REJECT_DUPLICATES;
        json_error_t load_error;
        json_t *item = json_loadf(input, jflags, &load_error);

        if (!item) {
            if (!feof(input)) {
                logmsg(ERROR, "JSON error at line %d, column %d: %s",
                       load_error.line, load_error.column, load_error.text);
            }

            continue;
        }

        if (!json_is_object(item)) {
            logmsg(ERROR, "Item %d in stream was not a JSON object; skipping",
                   item_count);
            error_count++;
            json_decref(item);
            continue;
        }

        baton_error_t error;
        json_t *result = fn(env, conn, item, args, &error);
        if (error.code != 0) {
            // On error, add an error report to the input JSON as a
            // property and print the input JSON
            error_count++;
            add_error_value(item, &error);
            print_json(item);
        }
        else {
            if (has_operation(item) && has_operation_target(item) && result) {
                // It's an envelope, so we add the result to the input
                // JSON as a property and print the input JSON
                baton_error_t rerror;
                add_result(item, result, &rerror);
                if (rerror.code != 0) {
                    logmsg(ERROR, "Failed to add error report to item %d "
                           "in stream. Error code %d: %s", item_count,
                           rerror.code, rerror.message);
                    error_count++;
                }
                print_json(item);
            }
            else if (result) {
                // There is no envelope and there is some result JSON,
                // so we print the result JSON
                print_json(result);
                json_decref(result);
            }
            else {
                // There is no envelope and it's a void operation
                // giving no result JSON, so we print the input JSON
                // instead
                print_json(item);
            }
        }

        if (args->flags & FLUSH) fflush(stdout);

        (*item_count)++;

        json_decref(item);
    } // while

    return error_count;
}

int do_operation(FILE *input, baton_json_op fn, operation_args_t *args) {
    int item_count  = 0;
    int error_count = 0;

    rodsEnv env;
    rcComm_t *conn = rods_login(&env);
    if (!conn) goto error;

    if (!input) goto error;

    error_count = iterate_json(input, &env, conn, fn, args, &item_count);
    if (error_count > 0) {
        logmsg(WARN, "Processed %d items with %d errors",
               item_count, error_count);
    }
    else {
        logmsg(DEBUG, "Processed %d items with %d errors",
               item_count, error_count);
    }

    rcDisconnect(conn);

    return error_count;

error:
    if (conn) rcDisconnect(conn);

    logmsg(ERROR, "Processed %d items with %d errors",
           item_count, error_count);

    return 1;
}

json_t *baton_json_dispatch_op(rodsEnv *env, rcComm_t *conn, json_t *envelope,
                               operation_args_t *args, baton_error_t *error) {
    json_t *result  = NULL;

    const char *op = get_operation_name(envelope, error);
    if (error->code != 0) goto error;

    json_t *target = get_operation_target(envelope, error);
    if (error->code != 0) goto error;

    operation_args_t args_copy = { .flags       = args->flags,
                                   .buffer_size = args->buffer_size,
                                   .zone_name   = args->zone_name };

    if (has_operation_params(envelope)) {
        json_t *params = get_operation_params(envelope, error);
        if (error->code != 0)  goto error;

        option_flags flags = args_copy.flags;
        if (acl_p(params))        flags = flags | PRINT_ACL;
        if (avu_p(params))        flags = flags | PRINT_AVU;
        if (checksum_p(params))   flags = flags | PRINT_CHECKSUM;
        if (contents_p(params))   flags = flags | PRINT_CONTENTS;
        if (replicate_p(params))  flags = flags | PRINT_REPLICATE;
        if (timestamp_p(params))  flags = flags | PRINT_TIMESTAMP;
        if (collection_p(params)) flags = flags | SEARCH_COLLECTIONS;
        if (object_p(params))     flags = flags | SEARCH_OBJECTS;
        args_copy.flags = flags;

        if (has_operation_arg(params)) {
            const char *arg = get_operation_arg(params, error);
            if (error->code != 0) goto error;

            logmsg(DEBUG, "Detected operation argument '%s'", arg);
            if (str_equals(arg, JSON_ARG_META_ADD, MAX_STR_LEN)) {
                args_copy.flags = flags | ADD_AVU;
            }
            else if (str_equals(arg, JSON_ARG_META_REM, MAX_STR_LEN)) {
                args_copy.flags = flags | REMOVE_AVU;
            }
            else {
                set_baton_error(error, -1,
                                "Invalid baton operation argument '%s'", arg);
              goto error;
            }
        }

        if (has_operation_path(params)) {
            const char *path = get_operation_path(params, error);
            if (error->code != 0) goto error;

            char *tmp = copy_str(path, MAX_STR_LEN);
            if (!tmp) {
                set_baton_error(error, errno, "Failed to copy string '%s'",
                                path);
                goto error;
            }

            args_copy.path = tmp;
        }
    }

    if (str_equals(op, JSON_CHMOD_OPERATION, MAX_STR_LEN)) {
        logmsg(DEBUG, "Dispatching to operation '%s'", op);
        result = baton_json_chmod_op(env, conn, target, &args_copy, error);
    }
    else if (str_equals(op, JSON_LIST_OPERATION, MAX_STR_LEN)) {
        logmsg(DEBUG, "Dispatching to operation '%s'", op);
        result = baton_json_list_op(env, conn, target, &args_copy, error);
    }
    else if (str_equals(op, JSON_METAMOD_OPERATION, MAX_STR_LEN)) {
        logmsg(DEBUG, "Dispatching to operation '%s'", op);
        result = baton_json_metamod_op(env, conn, target, &args_copy, error);
    }
    else if (str_equals(op, JSON_METAQUERY_OPERATION, MAX_STR_LEN)) {
        logmsg(DEBUG, "Dispatching to operation '%s'", op);
        result = baton_json_metaquery_op(env, conn, target, &args_copy, error);
    }
    else if (str_equals(op, JSON_GET_OPERATION, MAX_STR_LEN)) {
        logmsg(DEBUG, "Dispatching to operation '%s'", op);
        result = baton_json_get_op(env, conn, target, &args_copy, error);
    }
    else if (str_equals(op, JSON_PUT_OPERATION, MAX_STR_LEN)) {
        logmsg(DEBUG, "Dispatching to operation '%s'", op);
        result = baton_json_put_op(env, conn, target, &args_copy, error);
    }
    else if (str_equals(op, JSON_MOVE_OPERATION, MAX_STR_LEN)) {
        logmsg(DEBUG, "Dispatching to operation '%s'", op);
        result = baton_json_move_op(env, conn, target, &args_copy, error);
    }
    else {
        set_baton_error(error, -1, "Invalid baton operation '%s'", op);
        goto error;
    }

    if (args_copy.path) free(args_copy.path);

    return result;

error:
    if (args_copy.path) free(args_copy.path);

    return result;
}

json_t *baton_json_list_op(rodsEnv *env, rcComm_t *conn, json_t *target,
                           operation_args_t *args, baton_error_t *error) {
    json_t *result = NULL;

    char *path = json_to_path(target, error);
    if (error->code != 0) goto error;

    rodsPath_t rods_path;
    resolve_rods_path(conn, env, &rods_path, path, args->flags, error);
    if (error->code != 0) goto error;

    result = list_path(conn, &rods_path, args->flags, error);
    if (error->code != 0) goto error;

    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);
    if (path) free(path);

    return result;

error:
    if (path) free(path);

    return result;
}

json_t *baton_json_chmod_op(rodsEnv *env, rcComm_t *conn, json_t *target,
                            operation_args_t *args, baton_error_t *error) {
    json_t *result = NULL;

    char *path = json_to_path(target, error);
    if (error->code != 0) goto error;

    json_t *perms = json_object_get(target, JSON_ACCESS_KEY);
    if (!json_is_array(perms)) {
        set_baton_error(error, -1, "Permissions data for %s is not in "
                        "a JSON array", path);
        goto error;
    }

    rodsPath_t rods_path;
    resolve_rods_path(conn, env, &rods_path, path, args->flags, error);
    if (error->code != 0) goto error;

    recursive_op recurse = (args->flags & RECURSIVE) ? RECURSE : NO_RECURSE;

    for (size_t i = 0; i < json_array_size(perms); i++) {
        json_t *perm = json_array_get(perms, i);
        modify_json_permissions(conn, &rods_path, recurse, perm, error);

        if (error->code != 0) goto error;
    }

    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);
    if (path) free(path);

    return result;

error:
    if (path) free(path);

    return result;
}

json_t *baton_json_metaquery_op(rodsEnv *env, rcComm_t *conn, json_t *target,
                                operation_args_t *args, baton_error_t *error) {
    json_t *result = NULL;

    if (has_collection(target)) {
        resolve_collection(target, conn, env, args->flags, error);
        if (error->code != 0) goto error;
    }

    char *zone_name = args->zone_name;
    logmsg(DEBUG, "Metadata query in zone '%s'", zone_name);

    result = search_metadata(conn, target, zone_name, args->flags, error);
    if (error->code != 0) goto error;

    return result;

error:
    return result;
}

json_t *baton_json_metamod_op(rodsEnv *env, rcComm_t *conn, json_t *target,
                              operation_args_t *args, baton_error_t *error) {
    json_t *result = NULL;

    char *path = json_to_path(target, error);
    if (error->code != 0) goto error;

    json_t *avus = json_object_get(target, JSON_AVUS_KEY);
    if (!json_is_array(avus)) {
        set_baton_error(error, -1, "AVU data for %s is not in a JSON array",
                        path);
        goto error;
    }

    rodsPath_t rods_path;
    resolve_rods_path(conn, env, &rods_path, path, args->flags, error);
    if (error->code != 0) goto error;

    metadata_op operation;
    if (args->flags & ADD_AVU) {
        operation = META_ADD;
    }
    else if (args->flags & REMOVE_AVU) {
        operation = META_REM;
    }
    else {
        set_baton_error(error, -1, "No metadata operation was specified "
                        " for '%s'", path);
        goto error;
    }

    for (size_t i = 0; i < json_array_size(avus); i++) {
        json_t *avu = json_array_get(avus, i);
        modify_json_metadata(conn, &rods_path, operation, avu, error);
        if (error->code != 0) goto error;
    }

    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);
    if (path) free(path);

    return result;

error:
    if (path) free(path);

    return result;
}

json_t *baton_json_get_op(rodsEnv *env, rcComm_t *conn, json_t *target,
                          operation_args_t *args, baton_error_t *error) {
    json_t *result = NULL;

    char *path = json_to_path(target, error);
    if (error->code != 0) goto error;

    rodsPath_t rods_path;
    resolve_rods_path(conn, env, &rods_path, path, args->flags, error);
    if (error->code != 0) goto error;

    size_t bsize = args->buffer_size;
    logmsg(DEBUG, "Using a 'get' buffer size of %zu bytes", bsize);

    if (args->flags & SAVE_FILES) {
        char *file = NULL;
        file = json_to_local_path(target, error);
        if (error->code != 0) goto error;

        get_data_obj_file(conn, &rods_path, file, bsize, error);
        free(file);
        if (error->code != 0) goto error;
    }
    else if (args->flags & PRINT_RAW) {
        get_data_obj_stream(conn, &rods_path, stdout, bsize, error);
        if (error->code != 0) goto error;
    }
    else {
        result = ingest_data_obj(conn, &rods_path, args->flags, bsize, error);
        if (error->code != 0) goto error;
    }

    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);
    if (path) free(path);

    return result;

error:
    if (path) free(path);

    return result;
}

json_t *baton_json_write_op(rodsEnv *env, rcComm_t *conn, json_t *target,
                            operation_args_t *args, baton_error_t *error) {
    json_t *result = NULL;

    char *path = json_to_path(target, error);
    if (error->code != 0) goto error;

    rodsPath_t rods_path;
    resolve_rods_path(conn, env, &rods_path, path, args->flags, error);
    if (error->code != 0) goto error;

    char *file = json_to_local_path(target, error);
    if (error->code != 0) goto error;

    size_t bsize = args->buffer_size;
    logmsg(DEBUG, "Using a 'put' buffer size of %zu bytes", bsize);

    FILE *in = fopen(file, "r");
    if (!in) {
        set_baton_error(error, errno,
                        "Failed to open '%s' for reading: error %d %s",
                        "README", errno, strerror(errno));
        goto error;
    }

    write_data_obj(conn, in, &rods_path, bsize, error);
    int status = fclose(in);

    if (error->code != 0) goto error;
    if (status != 0) {
        set_baton_error(error, errno,
                        "Failed to close '%s': error %d %s",
                        "README", errno, strerror(errno));
        goto error;
    }

    if (path) free(path);

    return result;

error:
    if (path) free(path);

    return result;
}

json_t *baton_json_put_op(rodsEnv *env, rcComm_t *conn, json_t *target,
                          operation_args_t *args, baton_error_t *error) {
    json_t *result = NULL;

    char *path = json_to_path(target, error);
    if (error->code != 0) goto error;

    rodsPath_t rods_path;
    resolve_rods_path(conn, env, &rods_path, path, args->flags, error);
    if (error->code != 0) goto error;

    char *file = json_to_local_path(target, error);
    if (error->code != 0) goto error;

    int status = put_data_obj(conn, file, &rods_path, args->flags, error);

    if (error->code != 0) goto error;
    if (status != 0) {
        set_baton_error(error, errno,
                        "Failed to close '%s': error %d %s",
                        "README", errno, strerror(errno));
        goto error;
    }

    if (path) free(path);

    return result;

error:
    if (path) free(path);

    return result;
}

json_t *baton_json_move_op(rodsEnv *env, rcComm_t *conn, json_t *target,
                           operation_args_t *args, baton_error_t *error) {
    json_t *result = NULL;
    char *path     = NULL;

    path = json_to_path(target, error);
    if (error->code != 0) goto error;

    rodsPath_t rods_path;
    resolve_rods_path(conn, env, &rods_path, path, args->flags, error);
    if (error->code != 0) goto error;

    char *new_path = args->path;
    logmsg(DEBUG, "Moving '%s' to '%s'", path, new_path);

    move_rods_path(conn, &rods_path, new_path, error);
    if (error->code != 0) goto error;

    if (path) free(path);

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
