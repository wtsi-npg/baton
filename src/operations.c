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
        json_t *target = json_loadf(input, jflags, &load_error);

        if (!target) {
            if (!feof(input)) {
                logmsg(ERROR, "JSON error at line %d, column %d: %s",
                       load_error.line, load_error.column, load_error.text);
            }

            continue;
        }

        if (!json_is_object(target)) {
            logmsg(ERROR, "Item %d in stream was not a JSON object; skipping",
                   item_count);
            error_count++;
            json_decref(target);
            continue;
        }

        va_list fnargs;
        va_copy(fnargs, args);

        baton_error_t error;
        int status = fn(env, conn, target, flags, &error, fnargs);
        if (status != 0) error_count++;

        va_end(fnargs);

        (*item_count)++;

        if (flags & FLUSH) fflush(stdout);
        json_decref(target);
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

int baton_json_list_op(rodsEnv *env, rcComm_t *conn, json_t *target,
                       option_flags flags, baton_error_t *error,
                       va_list args) {
    (void) args; // Not used

    char *path = json_to_path(target, error);
    if (add_error_report(target, error)) {
        print_json(target);
        goto error;
    }

    rodsPath_t rods_path;
    resolve_rods_path(conn, env, &rods_path, path, flags, error);
    if (add_error_report(target, error)) {
        print_json(target);
        goto error;
    }

    json_t *results = list_path(conn, &rods_path, flags, error);
    if (add_error_report(target, error)) {
        print_json(target);
        goto error;
    }

    print_json(results);
    json_decref(results);

    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);
    if (path) free(path);

    return error->code;

error:
    if (path) free(path);

    return error->code;
}

int baton_json_chmod_op(rodsEnv *env, rcComm_t *conn, json_t *target,
                        option_flags flags, baton_error_t *error,
                        va_list args) {
    (void) args; // Not used

    char *path = json_to_path(target, error);
    if (add_error_report(target, error)) {
        print_json(target);
        goto error;
    }

    json_t *perms = json_object_get(target, JSON_ACCESS_KEY);
    if (!json_is_array(perms)) {
        set_baton_error(error, -1, "Permissions data for %s is not in "
                        "a JSON array", path);
        add_error_report(target, error);
        print_json(target);
        goto error;
    }

    rodsPath_t rods_path;
    resolve_rods_path(conn, env, &rods_path, path, flags, error);
    if (add_error_report(target, error)) {
        print_json(target);
        goto error;
    }

    recursive_op recurse = (flags & RECURSIVE) ? RECURSE : NO_RECURSE;

    for (size_t i = 0; i < json_array_size(perms); i++) {
        json_t *perm = json_array_get(perms, i);
        modify_json_permissions(conn, &rods_path, recurse, perm, error);
        if (add_error_report(target, error)) {
            print_json(target);
            goto error;
        }
    }

    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);
    if (path) free(path);

    return error->code;

error:
    if (path) free(path);

    return error->code;
}

int baton_json_metaquery_op(rodsEnv *env, rcComm_t *conn, json_t *target,
                            option_flags flags, baton_error_t *error,
                            va_list args) {
    json_t *results = NULL;

    if (has_collection(target)) {
        resolve_collection(target, conn, env, flags, error);

        if (add_error_report(target, error)) {
            print_json(target);
            goto error;
        }
    }

    char *zone_name = va_arg(args, char *);
    logmsg(DEBUG, "Metadata query in zone '%s'", zone_name);

    results = search_metadata(conn, target, zone_name, flags, error);

    if (add_error_report(target, error)) {
        print_json(target);
        goto error;
    }

    print_json(results);

    if (results) json_decref(results);

    return error->code;

error:
    return error->code;
}

int baton_json_metamod_op(rodsEnv *env, rcComm_t *conn, json_t *target,
                          option_flags flags, baton_error_t *error,
                          va_list args) {
    (void) args; // Not used

    char *path = json_to_path(target, error);
    if (add_error_report(target, error)) {
        print_json(target);
        goto error;
    }

    json_t *avus = json_object_get(target, JSON_AVUS_KEY);
    if (!json_is_array(avus)) {
        set_baton_error(error, -1, "AVU data for %s is not in a JSON array",
                        path);
        add_error_report(target, error);
        print_json(target);
        goto error;
    }

    rodsPath_t rods_path;
    resolve_rods_path(conn, env, &rods_path, path, flags, error);
    if (add_error_report(target, error)) {
        print_json(target);
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
        set_baton_error(error, -1, "No metadata operation was requested",
                        path);
        add_error_report(target, error);
        print_json(target);
        goto error;
    }

    for (size_t i = 0; i < json_array_size(avus); i++) {
        json_t *avu = json_array_get(avus, i);
        modify_json_metadata(conn, &rods_path, operation, avu, error);
        if (add_error_report(target, error)) {
            print_json(target);
            goto error;
        }
    }

    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);

    return error->code;

error:
    if (path) free(path);

    return error->code;
}

int baton_json_get_op(rodsEnv *env, rcComm_t *conn, json_t *target,
                      option_flags flags, baton_error_t *error,
                      va_list args) {
    char *path = json_to_path(target, error);
    if (add_error_report(target, error)) {
        print_json(target);
        goto error;
    }

    rodsPath_t rods_path;
    resolve_rods_path(conn, env, &rods_path, path, flags, error);
    if (add_error_report(target, error)) {
        print_json(target);
        goto error;
    }

    FILE *report_stream = stdout;

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

        if (add_error_report(target, error)) {
            print_json_stream(target, report_stream);
            goto error;
        }
    }
    else if (flags & PRINT_RAW) {
        report_stream = stderr;
        write_path_to_stream(conn, &rods_path, stdout, buffer_size, error);

        if (add_error_report(target, error)) {
            print_json_stream(target, report_stream);
            goto error;
        }
    }
    else {
        json_t *results =
            ingest_path(conn, &rods_path, flags, buffer_size, error);
        if (add_error_report(target, error)) {
            print_json_stream(target, report_stream);
            goto error;
        }
        else {
            print_json_stream(results, report_stream);
            json_decref(results);
        }
    }

    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);

    return error->code;

error:
    if (path) free(path);

    return error->code;
}
