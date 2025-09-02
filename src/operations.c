/**
 * Copyright (C) 2017, 2018, 2019, 2020, 2021, 2022, 2024, 2025 Genome
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
 * @file operations.c
 * @author Keith James <kdj@sanger.ac.uk>, Rob Davies <rmd@sanger.ac.uk>
 */

#include "config.h"
#include "time.h"

#include "baton.h"
#include "json.h"
#include "operations.h"
#include "signal_handler.h"
#include "write.h"
#include "utilities.h"

// Mutex protecting the session and the run_timeout_thread flag
pthread_mutex_t session_mutex = PTHREAD_MUTEX_INITIALIZER;
// While true, the client will continue to run the timeout thread
int run_timeout_thread = 1;
// Condition variable to exit the timeout thread when work is complete
pthread_cond_t watchdog_cond = PTHREAD_COND_INITIALIZER;

// Refresh the connection every timeout seconds
void* connection_timeout(void *session) {
    baton_session_t *sess = session;

    struct timespec abs_timeout;

    pthread_mutex_lock(&session_mutex);
    while (run_timeout_thread) {
        clock_gettime(CLOCK_REALTIME, &abs_timeout);
        abs_timeout.tv_sec += sess->max_connect_time;

        int status;
        do {
            status = pthread_cond_timedwait(&watchdog_cond, &session_mutex, &abs_timeout);
        }
        while (status == EINTR);

        if (status == ETIMEDOUT) {
            baton_disconnect(sess);
            logmsg(NOTICE, "Closed the iRODS connection after a timeout " "of %d seconds",
                   sess->max_connect_time);
        }
    }
    pthread_mutex_unlock(&session_mutex);

    return 0;
}

static int iterate_json(FILE *input,
                        baton_session_t *session,
                        const baton_json_op fn,
                        operation_args_t *args,
                        int *item_count,
                        int *error_count) {
    int status = 0;
    pthread_t tid;
    int thread_status = -1;

    long timeout = args->max_connect_time;
    if (timeout < 10) {
        logmsg(ERROR, "The connection timeout (--connect-time argument) "
               "must be >=10 seconds");
        status = 1;
        goto finally;
    }
    session->max_connect_time = timeout;

    thread_status = pthread_create(&tid, NULL, &connection_timeout, session);
    if (thread_status != 0) {
        logmsg(ERROR, "Failed to start connection management thread: %d", thread_status);
        goto finally;
    }

    while (!exit_flag && !feof(input)) {
        const size_t jflags = JSON_DISABLE_EOF_CHECK | JSON_REJECT_DUPLICATES;
        json_error_t load_error;
        json_t *item = json_loadf(input, jflags, &load_error); // JSON alloc

        if (!item) {
            if (!feof(input)) {
                logmsg(ERROR, "JSON error at line %d, column %d: %s", load_error.line,
                       load_error.column, load_error.text);
            }
            continue;
        }

        if (!json_is_object(item)) {
            logmsg(ERROR, "Item %d in stream was not a JSON object; skipping",
                   item_count);
            (*error_count)++;
            json_decref(item);
            continue;
        }

        pthread_mutex_lock(&session_mutex); // Lock before connecting and executing a job
        logmsg(DEBUG, "Work to do, session lock obtained");
        if (!session->conn) {
            logmsg(NOTICE, "Opening a new iRODS connection");
            status = baton_connect(session);
            if (status < 0) {
                pthread_mutex_unlock(&session_mutex);
                goto finally;
            }
        }

        baton_error_t error;
        json_t *result = fn(session, item, args, &error);
        pthread_mutex_unlock(&session_mutex); // Unlock before processing the result
        logmsg(DEBUG, "Work done, session lock released");

        if (error.code != 0) {
            // On error, add an error report to the input JSON as a
            // property and print the input JSON. A NULL result should
            // always be an error.
            (*error_count)++;
            add_error_value(item, &error);
            print_json(item);
        }
        else {
            if (has_operation(item) && has_operation_target(item)) {
                // It's an envelope, so we add the result to the input
                // JSON as a property and print the input JSON, The
                // result will be freed as part of the input JSON.
                baton_error_t rerror;
                add_result(item, result, &rerror);
                if (rerror.code != 0) {
                    logmsg(ERROR, "Failed to add error report to item %d "
                           "in stream. Error code %d: %s", item_count, rerror.code,
                           rerror.message);
                    (*error_count)++;
                }
                print_json(item);
            }
            else {
                // There is no envelope and there is some result JSON,
                // so we print the result JSON. The result is not
                // freed as part of the input JSON, so we free it here.
                print_json(result);
                json_decref(result);
            }
        }

        if (args->flags & FLUSH) fflush(stdout);

        (*item_count)++;

        json_decref(item); // JSON free
    }                      // while

    if (exit_flag) {
        status = exit_flag;
        logmsg(WARN, "Exiting on signal with code %d", exit_flag);
    }

finally:
    pthread_mutex_lock(&session_mutex);
    run_timeout_thread = 0;
    pthread_cond_signal(&watchdog_cond); // Unblock the thread waiting on cond

    baton_disconnect(session);
    logmsg(NOTICE, "Closed the connection on exit")
    pthread_mutex_unlock(&session_mutex);

    if (thread_status == 0) {
        status = pthread_join(tid, NULL);
        if (status != 0) {
            logmsg(ERROR, "Timeout thread failed to join: %s", strerror(status));
        }
    }

    return status;
}

int do_operation(FILE *input, const baton_json_op fn, operation_args_t *args) {
    int item_count  = 0;
    int error_count = 0;
    int status      = 0;

    baton_session_t *session = new_baton_session();

    if (!input) {
        status = 1;
        goto error;
    }

    status = iterate_json(input, session, fn, args, &item_count, &error_count);
    if (status != 0) goto error;

    if (error_count > 0) {
        logmsg(WARN, "Processed %d items with %d errors", item_count, error_count);
        status = 1;
    }
    else {
        logmsg(DEBUG, "Processed %d items with %d errors", item_count, error_count);
    }

    free_baton_session(session);;

    return status;

error:
    logmsg(ERROR, "Processed %d items with %d errors", item_count, error_count);

    free_baton_session(session);

    return status;
}

json_t* baton_json_dispatch_op(baton_session_t *session,
                               json_t *envelope,
                               const operation_args_t *args,
                               baton_error_t *error) {
    json_t *result = NULL;

    operation_args_t args_copy = {
        .flags = args->flags,
        .buffer_size = args->buffer_size,
        .zone_name = args->zone_name,
        .path = NULL
    };

    const char *op = get_operation(envelope, error);
    if (error->code != 0) goto finally;

    if (!op) {
        set_baton_error(error, -1, "No baton operation given");
        goto finally;
    }

    json_t *target = get_operation_target(envelope, error);
    if (error->code != 0) goto finally;

    if (has_operation(envelope)) {
        const json_t *jargs = get_operation_args(envelope, error);
        if (error->code != 0) goto finally;

        option_flags flags = args_copy.flags;
        if (op_acl_p(jargs)) flags = flags | PRINT_ACL;
        if (op_avu_p(jargs)) flags = flags | PRINT_AVU;
        if (op_print_checksum_p(jargs)) flags = flags | PRINT_CHECKSUM;
        if (op_calculate_checksum_p(jargs)) flags = flags | CALCULATE_CHECKSUM |
            PRINT_CHECKSUM;
        if (op_verify_checksum_p(jargs)) flags = flags | VERIFY_CHECKSUM | PRINT_CHECKSUM;
        if (op_contents_p(jargs)) flags = flags | PRINT_CONTENTS;
        if (op_replicate_p(jargs)) flags = flags | PRINT_REPLICATE;
        if (op_redirect_to_server_p(jargs)) flags = flags | REDIRECT_TO_SERVER;
        if (op_size_p(jargs)) flags = flags | PRINT_SIZE;
        if (op_timestamp_p(jargs)) flags = flags | PRINT_TIMESTAMP;
        if (op_raw_p(jargs)) flags = flags | PRINT_RAW;
        if (op_save_p(jargs)) flags = flags | SAVE_FILES;
        if (op_recurse_p(jargs)) flags = flags | RECURSIVE;
        if (op_force_p(jargs)) flags = flags | FORCE;
        if (op_collection_p(jargs)) flags = flags | SEARCH_COLLECTIONS;
        if (op_object_p(jargs)) flags = flags | SEARCH_OBJECTS;
        if (op_single_server_p(jargs)) flags = flags | SINGLE_SERVER;
        args_copy.flags = flags;

        if (has_operation(jargs)) {
            const char *arg = get_operation(jargs, error);
            if (error->code != 0) goto finally;

            logmsg(DEBUG, "Detected operation '%s'", op);
            if (str_equals(arg, JSON_ARG_META_ADD, MAX_STR_LEN)) {
                args_copy.flags = flags | ADD_AVU;
            }
            else if (str_equals(arg, JSON_ARG_META_REM, MAX_STR_LEN)) {
                args_copy.flags = flags | REMOVE_AVU;
            }
            else {
                set_baton_error(error, -1, "Invalid baton operation argument '%s'", arg);
                goto finally;
            }
        }

        if (has_op_path(jargs)) {
            const char *path = get_op_path(jargs, error);
            if (error->code != 0) goto finally;

            char *tmp = copy_str(path, MAX_STR_LEN);
            if (!tmp) {
                set_baton_error(error, errno, "Failed to copy string '%s'", path);
                goto finally;
            }

            args_copy.path = tmp;
        }
    }

    logmsg(DEBUG, "Dispatching to operation '%s'", op);

    if (str_equals(op, JSON_CHMOD_OP, MAX_STR_LEN)) {
        result = baton_json_chmod_op(session, target, &args_copy, error);
    }
    else if (str_equals(op, JSON_CHECKSUM_OP, MAX_STR_LEN)) {
        result = baton_json_checksum_op(session, target, &args_copy, error);
        if (error->code != 0) goto finally;

        if (args_copy.flags & PRINT_CHECKSUM) {
            result = add_checksum_json_object(session->conn, result, error);
            if (error->code != 0) goto finally;
        }
    }
    else if (str_equals(op, JSON_LIST_OP, MAX_STR_LEN)) {
        result = baton_json_list_op(session, target, &args_copy, error);
        if (error->code != 0) goto finally;
    }
    else if (str_equals(op, JSON_METAMOD_OP, MAX_STR_LEN)) {
        result = baton_json_metamod_op(session, target, &args_copy, error);
    }
    else if (str_equals(op, JSON_METAQUERY_OP, MAX_STR_LEN)) {
        result = baton_json_metaquery_op(session, target, &args_copy, error);
    }
    else if (str_equals(op, JSON_GET_OP, MAX_STR_LEN)) {
        result = baton_json_get_op(session, target, &args_copy, error);
    }
    else if (str_equals(op, JSON_PUT_OP, MAX_STR_LEN)) {
        if (args_copy.flags & SINGLE_SERVER) {
            logmsg(DEBUG, "Single-server mode, falling back " "to operation 'write'");
            result = baton_json_write_op(session, target, &args_copy, error);
        }
        else {
            result = baton_json_put_op(session, target, &args_copy, error);
        }
        if (error->code != 0) goto finally;

        if (args_copy.flags & PRINT_CHECKSUM) {
            result = add_checksum_json_object(session->conn, result, error);
            if (error->code != 0) goto finally;
        }
    }
    else if (str_equals(op, JSON_MOVE_OP, MAX_STR_LEN)) {
        result = baton_json_move_op(session, target, &args_copy, error);
    }
    else if (str_equals(op, JSON_RM_OP, MAX_STR_LEN)) {
        result = baton_json_rm_op(session, target, &args_copy, error);
    }
    else if (str_equals(op, JSON_MKCOLL_OP, MAX_STR_LEN)) {
        result = baton_json_mkcoll_op(session, target, &args_copy, error);
    }
    else if (str_equals(op, JSON_RMCOLL_OP, MAX_STR_LEN)) {
        result = baton_json_rmcoll_op(session, target, &args_copy, error);
    }
    else {
        set_baton_error(error, -1, "Invalid baton operation '%s'", op);
    }

finally:
    if (args_copy.path) free(args_copy.path);

    return result;
}

json_t* baton_json_list_op(baton_session_t *session,
                           json_t *target,
                           const operation_args_t *args,
                           baton_error_t *error) {
    json_t *result = NULL;

    char *path = json_to_path(target, error);
    if (error->code != 0) goto finally;

    rodsPath_t rods_path = {0};
    resolve_rods_path(session, &rods_path, path, args->flags, error);
    if (error->code != 0) goto finally;

    result = list_path(session->conn, &rods_path, args->flags, error);
    if (error->code != 0) goto finally;

finally:
    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);
    if (path) free(path);

    return result;
}

json_t* baton_json_chmod_op(baton_session_t *session,
                            json_t *target,
                            const operation_args_t *args,
                            baton_error_t *error) {
    json_t *result = NULL;

    char *path = json_to_path(target, error);
    if (error->code != 0) goto finally;

    rodsPath_t rods_path = {0};
    resolve_rods_path(session, &rods_path, path, args->flags, error);
    if (error->code != 0) goto finally;

    const json_t *perms = json_object_get(target, JSON_ACCESS_KEY);
    if (!json_is_array(perms)) {
        set_baton_error(error, -1, "Permissions data for %s is not in " "a JSON array",
                        path);
        goto finally;
    }

    const recursive_op recurse = (args->flags & RECURSIVE) ? RECURSE : NO_RECURSE;

    for (size_t i = 0; i < json_array_size(perms); i++) {
        json_t *perm = json_array_get(perms, i);
        modify_json_permissions(session->conn, &rods_path, recurse, perm, error);

        if (error->code != 0) goto finally;
    }

    result = json_deep_copy(target);
    if (!result) {
        set_baton_error(error, -1, "Internal error: failed to deep-copy " "result for %s",
                        path);
    }

finally:
    if (path) free(path);
    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);

    return result;
}

json_t* baton_json_checksum_op(baton_session_t *session,
                               json_t *target,
                               const operation_args_t *args,
                               baton_error_t *error) {
    json_t *result    = NULL;
    char *checksum    = NULL;
    json_t *jchecksum = NULL;

    char *path = json_to_path(target, error);
    if (error->code != 0) goto finally;

    rodsPath_t rods_path = {0};
    resolve_rods_path(session, &rods_path, path, args->flags, error);
    if (error->code != 0) goto finally;

    if (!represents_data_object(target)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT, "cannot checksum a non-data-object");
        goto finally;
    }

    const option_flags flags = args->flags;
    checksum                 = checksum_data_obj(session->conn, &rods_path, flags, error);
    if (error->code != 0) goto finally;

    jchecksum = checksum_to_json(checksum, error);
    if (error->code != 0) goto finally;

    add_checksum(target, jchecksum, error);
    if (error->code != 0) {
        // Only free this on error. On success, it becomes owned by target
        json_decref(jchecksum);
        goto finally;
    }

    result = json_deep_copy(target);
    if (!result) {
        set_baton_error(error, -1, "Internal error: failed to deep-copy " "result for %s",
                        path);
    }

finally:
    if (path) free(path);
    if (checksum) free(checksum);
    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);

    return result;
}

json_t* baton_json_metaquery_op(baton_session_t *session,
                                json_t *target,
                                const operation_args_t *args,
                                baton_error_t *error) {
    json_t *result = NULL;

    if (has_collection(target)) {
        resolve_collection(session, target, args->flags, error);
        if (error->code != 0) goto finally;
    }

    char *zone_name = args->zone_name;
    logmsg(DEBUG, "Metadata query in zone '%s'", zone_name);

    result = search_metadata(session->conn, target, zone_name, args->flags, error);

finally:
    return result;
}

json_t* baton_json_metamod_op(baton_session_t *session,
                              json_t *target,
                              const operation_args_t *args,
                              baton_error_t *error) {
    json_t *result = NULL;

    char *path = json_to_path(target, error);
    if (error->code != 0) goto finally;

    rodsPath_t rods_path = {0};
    resolve_rods_path(session, &rods_path, path, args->flags, error);
    if (error->code != 0) goto finally;

    const json_t *avus = json_object_get(target, JSON_AVUS_KEY);
    if (!json_is_array(avus)) {
        set_baton_error(error, -1, "AVU data for %s is not in a JSON array", path);
        goto finally;
    }

    metadata_op operation;
    if (args->flags & ADD_AVU) {
        operation = META_ADD;
    }
    else if (args->flags & REMOVE_AVU) {
        operation = META_REM;
    }
    else {
        set_baton_error(error, -1, "No metadata operation was specified " " for '%s'",
                        path);
        goto finally;
    }

    for (size_t i = 0; i < json_array_size(avus); i++) {
        const json_t *avu = json_array_get(avus, i);
        modify_json_metadata(session->conn, &rods_path, operation, avu, error);
        if (error->code != 0) goto finally;
    }

    result = json_deep_copy(target);
    if (!result) {
        set_baton_error(error, -1, "Internal error: failed to deep-copy " "result for %s",
                        path);
    }

finally:
    if (path) free(path);
    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);

    return result;
}

json_t* baton_json_get_op(baton_session_t *session,
                          json_t *target,
                          const operation_args_t *args,
                          baton_error_t *error) {
    json_t *result = NULL;
    char *file     = NULL;

    char *path = json_to_path(target, error);
    if (error->code != 0) goto finally;

    rodsPath_t rods_path = {0};
    resolve_rods_path(session, &rods_path, path, args->flags, error);
    if (error->code != 0) goto finally;

    file = json_to_local_path(target, error);
    if (error->code != 0) goto finally;

    const size_t bsize = args->buffer_size;

    if (args->flags & SAVE_FILES) {
        result = json_deep_copy(target);
        if (!result) {
            set_baton_error(error, errno, "Failed to allocate memory for result");
            goto finally;
        }

        get_data_obj_file(session, &rods_path, file, args->flags, error);
        if (error->code != 0) goto finally;
    }
    else if (args->flags & PRINT_RAW) {
        result = json_deep_copy(target);
        if (!result) {
            set_baton_error(error, errno, "Failed to allocate memory for result");
            goto finally;
        }
        get_data_obj_stream(session, &rods_path, stdout, bsize, error);
        if (error->code != 0) goto finally;
    }
    else {
        result = ingest_data_obj(session, &rods_path, args->flags, bsize, error);
    }

finally:
    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);
    if (path) free(path);
    if (file) free(file);

    return result;
}

json_t* baton_json_write_op(baton_session_t *session,
                            json_t *target,
                            const operation_args_t *args,
                            baton_error_t *error) {
    char *file = NULL;
    char *path = json_to_path(target, error);
    if (error->code != 0) goto finally;

    rodsPath_t rods_path = {0};
    resolve_rods_path(session, &rods_path, path, args->flags, error);
    if (error->code != 0) goto finally;

    file = json_to_local_path(target, error);
    if (error->code != 0) goto finally;

    if (!represents_data_object(target)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "cannot write a data object given a non-data-object");
        goto finally;
    }

    const size_t bsize = args->buffer_size;
    logmsg(DEBUG, "Using a 'write' buffer size of %zu bytes", bsize);

    FILE *in = fopen(file, "r");
    if (!in) {
        set_baton_error(error, errno, "Failed to open '%s' for reading: error %d %s",
                        file, errno, strerror(errno));
        goto finally;
    }

    write_data_obj(session, in, &rods_path, bsize, args->flags, error);
    const int status = fclose(in);

    if (error->code != 0) goto finally;
    if (status != 0) {
        set_baton_error(error, errno, "Failed to close '%s': error %d %s", file, errno,
                        strerror(errno));
    }

finally:
    if (path) free(path);
    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);
    if (file) free(file);

    return target;
}

json_t* baton_json_put_op(baton_session_t *session,
                          json_t *target,
                          const operation_args_t *args,
                          baton_error_t *error) {
    json_t *result     = NULL;
    char *file         = NULL;
    char *def_resource = NULL;
    char *checksum     = NULL;

    char *path = json_to_path(target, error);
    if (error->code != 0) goto finally;

    rodsPath_t rods_path = {0};
    resolve_rods_path(session, &rods_path, path, args->flags, error);
    if (error->code != 0) goto finally;

    file = json_to_local_path(target, error);
    if (error->code != 0) goto finally;

    if (strnlen(session->env->rodsDefResource, NAME_LEN) > 0) {
        def_resource = session->env->rodsDefResource;
        logmsg(DEBUG, "Using default iRODS resource '%s'", def_resource);
    }

    if (has_checksum(target)) {
        checksum = json_to_checksum(target, error);
        if (error->code != 0) goto finally;
        logmsg(DEBUG, "Using supplied checksum '%s'", checksum);
    }

    const int status = put_data_obj(session, file, &rods_path, def_resource, checksum,
                                    args->flags, error);
    if (error->code != 0) goto finally;
    if (status != 0) {
        set_baton_error(error, errno, "Failed to close '%s': error %d %s", file, errno,
                        strerror(errno));
        goto finally;
    }

    result = json_deep_copy(target);
    if (!result) {
        set_baton_error(error, -1, "Internal error: failed to deep-copy " "result for %s",
                        path);
    }

finally:
    if (checksum) free(checksum);
    if (path) free(path);
    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);
    if (file) free(file);

    return result;
}

json_t* baton_json_move_op(baton_session_t *session,
                           json_t *target,
                           const operation_args_t *args,
                           baton_error_t *error) {
    json_t *result = NULL;

    char *path = json_to_path(target, error);
    if (error->code != 0) goto finally;

    rodsPath_t rods_path = {0};
    resolve_rods_path(session, &rods_path, path, args->flags, error);
    if (error->code != 0) goto finally;

    char *new_path = args->path;
    logmsg(DEBUG, "Moving '%s' to '%s'", path, new_path);

    move_rods_path(session->conn, &rods_path, new_path, error);
    if (error->code != 0) goto finally;

    result = json_deep_copy(target);
    if (!result) {
        set_baton_error(error, -1, "Internal error: failed to deep-copy " "result for %s",
                        path);
    }

finally:
    if (path) free(path);
    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);

    return result;
}

json_t* baton_json_rm_op(baton_session_t *session,
                         json_t *target,
                         const operation_args_t *args,
                         baton_error_t *error) {
    json_t *result = NULL;

    char *path = json_to_path(target, error);
    if (error->code != 0) goto finally;

    rodsPath_t rods_path = {0};
    resolve_rods_path(session, &rods_path, path, args->flags, error);
    if (error->code != 0) goto finally;

    if (!represents_data_object(target)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT, "cannot remove a non-data-object");
        goto finally;
    }

    logmsg(DEBUG, "Removing data object '%s'", path);
    remove_data_object(session->conn, &rods_path, args->flags, error);
    if (error->code != 0) goto finally;

    result = json_deep_copy(target);
    if (!result) {
        set_baton_error(error, -1, "Internal error: failed to deep-copy " "result for %s",
                        path);
    }

finally:
    if (path) free(path);
    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);

    return result;
}

json_t* baton_json_mkcoll_op(baton_session_t *session,
                             json_t *target,
                             const operation_args_t *args,
                             baton_error_t *error) {
    json_t *result = NULL;

    char *path = json_to_collection_path(target, error);
    if (error->code != 0) goto finally;

    rodsPath_t rods_path = {0};
    resolve_rods_path(session, &rods_path, path, args->flags, error);
    if (error->code != 0) goto finally;

    if (represents_data_object(target)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "cannot make a collection given a data object");
        goto finally;
    }

    logmsg(DEBUG, "Creating collection '%s'", path);
    create_collection(session->conn, &rods_path, args->flags, error);
    if (error->code != 0) goto finally;

    result = json_deep_copy(target);
    if (!result) {
        set_baton_error(error, -1, "Internal error: failed to deep-copy " "result for %s",
                        path);
    }

finally:
    if (path) free(path);
    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);

    return result;
}

json_t* baton_json_rmcoll_op(baton_session_t *session,
                             json_t *target,
                             const operation_args_t *args,
                             baton_error_t *error) {
    json_t *result = NULL;

    char *path = json_to_collection_path(target, error);
    if (error->code != 0) goto finally;

    rodsPath_t rods_path = {0};
    resolve_rods_path(session, &rods_path, path, args->flags, error);
    if (error->code != 0) goto finally;

    if (represents_data_object(target)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "cannot remove a collection given a data object");
        goto finally;
    }

    logmsg(DEBUG, "Removing collection '%s'", path);
    remove_collection(session->conn, &rods_path, args->flags, error);
    if (error->code != 0) goto finally;

    result = json_deep_copy(target);
    if (!result) {
        set_baton_error(error, -1, "Internal error: failed to deep-copy " "result for %s",
                        path);
    }

finally:
    if (path) free(path);
    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);

    return result;
}