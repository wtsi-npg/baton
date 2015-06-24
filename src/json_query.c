/**
 * Copyright (C) 2013, 2014, 2015 Genome Research Ltd. All rights
 * reserved.
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
 * @file json_query.c
 * @author Keith James <kdj@sanger.ac.uk>
 */

#include <stdlib.h>

#include <jansson.h>

#include "config.h"
#include "baton.h"
#include "json.h"
#include "json_query.h"
#include "log.h"
#include "query.h"
#include "utilities.h"

static size_t parse_attr_value(int column, const char *label,
                               const char *input, char *output,
                               size_t max_len);

static int is_zone_hint(const char *path);

static const char *map_access_level(const char *access_level,
                                    baton_error_t *error);

static const char *revmap_access_level(const char *icat_level);

void log_json_error(log_level level, json_error_t *error) {
    logmsg(level, "JSON error: %s, line %d, column %d, position %d",
           error->text, error->line, error->column, error->position);
}

const char *ensure_valid_operator(const char *oper, baton_error_t *error) {
    static size_t num_operators = 10;
    static char *operators[] = { SEARCH_OP_EQUALS,   SEARCH_OP_LIKE,
                                 SEARCH_OP_NOT_LIKE, SEARCH_OP_IN,
                                 SEARCH_OP_STR_GT,   SEARCH_OP_STR_LT,
                                 SEARCH_OP_NUM_GT,   SEARCH_OP_NUM_LT,
                                 SEARCH_OP_STR_GE,   SEARCH_OP_STR_LE,
                                 SEARCH_OP_NUM_GE,   SEARCH_OP_NUM_LE };
    size_t valid_index;
    int valid = 0;
    for (size_t i = 0; i < num_operators; i++) {
        if (str_equals_ignore_case(oper, operators[i], MAX_STR_LEN)) {
            valid = 1;
            valid_index = i;
            break;
        }
    }

    if (!valid) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid operator: expected one of "
                        "[%s, %s, %s, %s, %s, %s, %s, %s, %s, %s]",
                        SEARCH_OP_EQUALS,   SEARCH_OP_LIKE,
                        SEARCH_OP_NOT_LIKE, SEARCH_OP_IN,
                        SEARCH_OP_STR_GT,   SEARCH_OP_STR_LT,
                        SEARCH_OP_NUM_GT,   SEARCH_OP_NUM_LT,
                        SEARCH_OP_STR_GE,   SEARCH_OP_STR_LE,
                        SEARCH_OP_NUM_GE,   SEARCH_OP_NUM_LE);
        goto error;
    }

    return operators[valid_index];

error:
    return NULL;
}

json_t *do_search(rcComm_t *conn, char *zone_name, json_t *query,
                  query_format_in_t *format,
                  prepare_avu_search_cb prepare_avu,
                  prepare_acl_search_cb prepare_acl,
                  prepare_tps_search_cb prepare_cre,
                  prepare_tps_search_cb prepare_mod,
                  baton_error_t *error) {
    genQueryInp_t *query_in = NULL;
    char *zone_hint         = zone_name;
    char *root_path         = NULL;
    json_t *items           = NULL;
    json_t *avus;

    if (represents_collection(query)) {
        root_path = json_to_path(query, error);
        logmsg(DEBUG, "Query represents a collection: '%s'", root_path);
        if (error->code != 0) goto error;
    }

    query_in = make_query_input(SEARCH_MAX_ROWS, format->num_columns,
                                format->columns);

    if (root_path) {
        rodsPath_t rods_path;

        int status = set_rods_path(conn, &rods_path, root_path);
        if (status < 0) {
            set_baton_error(error, status, "Failed to set iRODS path '%s'",
                            root_path);
            goto error;
        }

        if (str_starts_with(root_path, "/", MAX_STR_LEN)) {
            // Is search path just a zone hint? e.g. "/seq"
            if (is_zone_hint(root_path)) {
                if (!zone_hint) {
                    zone_hint = root_path;
                    zone_hint++; // Skip the leading slash

                    logmsg(DEBUG, "Using zone hint from JSON: '%s'", zone_hint);
                }
            }
            else {
                logmsg(DEBUG, "Limiting search to path '%s'", root_path);
                query_in = prepare_path_search(query_in, root_path);
            }
        }
    }

    // AVUs are mandatory for searches
    avus = get_avus(query, error);
    if (error->code != 0) goto error;

    query_in = prepare_json_avu_search(query_in, avus, prepare_avu, error);
    if (error->code != 0) goto error;

    // Report latest replicate only
    if (format->latest) {
        query_in = limit_to_newest_repl(query_in);
    }

    // ACL is optional
    if (has_acl(query)) {
        json_t *acl = get_acl(query, error);
        if (error->code != 0) goto error;

        query_in = prepare_json_acl_search(query_in, acl, prepare_acl, error);
        if (error->code != 0) goto error;
    }

    // Timestamp is optional
    if (has_timestamps(query)) {
        json_t *tps = get_timestamps(query, error);
        if (error->code != 0) goto error;

        query_in = prepare_json_tps_search(query_in, tps, prepare_cre,
                                           prepare_mod, error);
        if (error->code != 0) goto error;
    }

    if (zone_hint) {
        logmsg(TRACE, "Setting zone to '%s'", zone_hint);
        addKeyVal(&query_in->condInput, ZONE_KW, zone_hint);
    }

    items = do_query(conn, query_in, format->labels, error);
    if (error->code != 0) goto error;

    if (root_path) free(root_path);
    free_query_input(query_in);
    logmsg(TRACE, "Found %d matching items", json_array_size(items));

    return items;

error:
    if (root_path) free(root_path);
    if (query_in)  free_query_input(query_in);
    if (items)     json_decref(items);

    return NULL;
}

json_t *do_query(rcComm_t *conn, genQueryInp_t *query_in,
                 const char *labels[], baton_error_t *error) {
    genQueryOut_t *query_out = NULL;
    size_t chunk_num  = 0;
    int continue_flag = 0;

    char *err_name;
    char *err_subname;
    int status;

    json_t *results = json_array();
    if (!results) {
        set_baton_error(error, -1, "Failed to allocate a new JSON array");
        goto error;
    }

    logmsg(DEBUG, "Running query ...");

    while (chunk_num == 0 || continue_flag > 0) {
        logmsg(DEBUG, "Attempting to get chunk %d of query", chunk_num);

        status = rcGenQuery(conn, query_in, &query_out);

        if (status == 0) {
            logmsg(DEBUG, "Successfully fetched chunk %d of query", chunk_num);

            if (!query_out) {
                set_baton_error(error, -1,
                                "Query result unexpectedly NULL "
                                "in chunk %d error %d", chunk_num, -1);
                goto error;
            }

            // Allows query_out to be freed
            continue_flag = query_out->continueInx;

            // Cargo-cult from iRODS clients; not sure this is useful
            query_in->continueInx = query_out->continueInx;

            json_t *chunk = make_json_objects(query_out, labels);
            if (!chunk) {
                set_baton_error(error, -1,
                                "Failed to convert query result to JSON: "
                                "in chunk %d error %d", chunk_num, -1);
                goto error;
            }

            logmsg(TRACE, "Converted query result to JSON: in chunk %d of %d",
                   chunk_num, json_array_size(chunk));
            chunk_num++;

            status = json_array_extend(results, chunk);
            json_decref(chunk);

            if (status != 0) {
                set_baton_error(error, status,
                                "Failed to add JSON query result to total: "
                                "in chunk %d error %d", chunk_num, status);
                goto error;
            }

            free_query_output(query_out);
        }
        else if (status == CAT_NO_ROWS_FOUND && chunk_num > 0) {
            // Oddly CAT_NO_ROWS_FOUND is also returned at the end of a
            // batch of chunks; test chunk_num to distinguish catch this
            logmsg(TRACE, "Got CAT_NO_ROWS_FOUND at end of results!");
            break;
        }
        else if (status == CAT_NO_ROWS_FOUND) {
            // If this genuinely means no rows have been found, should we
            // free this, or not? Current iRODS leaves this NULL.
            logmsg(TRACE, "Query returned no results");
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

    logmsg(DEBUG, "Obtained a total of %d JSON results in %d chunks",
           chunk_num, json_array_size(results));

    return results;

error:
    if (conn->rError) {
        logmsg(ERROR, error->message);
        log_rods_errstack(ERROR, conn->rError);
    }
    else {
        logmsg(ERROR, error->message);
    }

    if (query_out) free_query_output(query_out);
    if (results)   json_decref(results);

    return NULL;
}

json_t *make_json_objects(genQueryOut_t *query_out, const char *labels[]) {
    json_t *array = json_array();
    if (!array) {
        logmsg(ERROR, "Failed to allocate a new JSON array");
        goto error;
    }

    size_t num_rows = (size_t) query_out->rowCnt;
    logmsg(DEBUG, "Converting %d rows of results to JSON", num_rows);

    for (size_t row = 0; row < num_rows; row++) {
        logmsg(DEBUG, "Converting row %d of %d to JSON", row, num_rows);

        json_t *jrow = json_object();
        if (!jrow) {
            logmsg(ERROR, "Failed to allocate a new JSON object for "
                   "result row %d of %d", row, query_out->rowCnt);
            goto error;
        }

        size_t num_attr = query_out->attriCnt;
        for (size_t i = 0; i < num_attr; i++) {
            size_t len   = query_out->sqlResult[i].len;
            char *result = query_out->sqlResult[i].value;
            result += row * len;

            logmsg(DEBUG, "Encoding column %d '%s' value '%s' as JSON",
                   i, labels[i], result);

            // Skip any results which return as an empty string
            // (notably units, when they are absent from an AVU).
            if (strnlen(result, len) > 0) {
                size_t vlen = len * 2 + 1; // +1 includes NUL
                char value[vlen];
                memset(value, 0, sizeof value);

                if (parse_attr_value(i, labels[i], result, value, vlen) > 0) {
                    json_t *jvalue = json_pack("s", value);
                    if (!jvalue) goto error;

                    // TODO: check return value
                    json_object_set_new(jrow, labels[i], jvalue);
                }
            }
        }

        if (json_array_append_new(array, jrow) != 0) {
            logmsg(ERROR, "Failed to append a new JSON result at row %d of %d",
                   row, query_out->rowCnt);
            goto error;
        }
    }

    return array;

error:
    logmsg(ERROR, "Failed to convert result to JSON");

    if (array) json_decref(array);

    return NULL;
}

genQueryInp_t *prepare_json_acl_search(genQueryInp_t *query_in,
                                       json_t *mapped_acl,
                                       prepare_acl_search_cb prepare,
                                       baton_error_t *error) {
    size_t num_clauses = json_array_size(mapped_acl);
    if (num_clauses > 1) {
        set_baton_error(error, -1,
                        "Invalid permissions specification "
                        "(contains %d access elements); cannot query "
                        "on more than one access element at a time "
                        "due to limits in the iRODS general query interface",
                        num_clauses);
        goto error;
    }

    size_t i;
    json_t *access;
    json_array_foreach(mapped_acl, i, access) {
        if (!json_is_object(access)) {
            set_baton_error(error, CAT_INVALID_ARGUMENT,
                            "Invalid permissions specification at position "
                            "%d of %d: not a JSON object", i, num_clauses);
            goto error;
        }

        const char *owner_name = get_access_owner(access, error);
        if (error->code != 0) goto error;

        const char *access_level = get_access_level(access, error);
        if (error->code != 0) goto error;

        query_in = prepare(query_in, owner_name, access_level);
    }

    return query_in;

error:
    return query_in;
}

genQueryInp_t *prepare_json_avu_search(genQueryInp_t *query_in,
                                       json_t *avus,
                                       prepare_avu_search_cb prepare,
                                       baton_error_t *error) {
    json_t *in_opvalue = NULL;
    size_t num_clauses = json_array_size(avus);

    size_t i;
    json_t *avu;
    json_array_foreach(avus, i, avu) {
        if (!json_is_object(avu)) {
            set_baton_error(error, CAT_INVALID_ARGUMENT,
                            "Invalid AVU at position %d of %d: ",
                            "not a JSON object", i, num_clauses);
            goto error;
        }

        const char *attr_name = get_avu_attribute(avu, error);
        if (error->code != 0) goto error;

        const char *oper = get_avu_operator(avu, error);
        if (error->code != 0) goto error;

        if (!oper) {
            oper = SEARCH_OP_EQUALS;
        }

        const char *valid_oper = ensure_valid_operator(oper, error);
        if (error->code != 0) goto error;

        const char *attr_value;
        if (str_equals_ignore_case(oper, SEARCH_OP_IN, MAX_STR_LEN)) {
            // this is an IN query, parse value as JSON array instead of string
            attr_value = make_in_op_value(avu, error);
            if (error->code != 0) goto error;
        } else {
            attr_value = get_avu_value(avu, error);
            if (error->code != 0) goto error;
        }

        logmsg(DEBUG, "Preparing AVU search a: '%s' v: '%s', op: '%s'",
               attr_name, attr_value, valid_oper);

        prepare(query_in, attr_name, attr_value, valid_oper);

        if (in_opvalue) {
            json_decref(in_opvalue);
            in_opvalue = NULL; // Reset for any subsequent IN clause
        }
     }

    return query_in;

error:
    if (in_opvalue) json_decref(in_opvalue);
    return query_in;
}

genQueryInp_t *prepare_json_tps_search(genQueryInp_t *query_in,
                                       json_t *timestamps,
                                       prepare_tps_search_cb prepare_cre,
                                       prepare_tps_search_cb prepare_mod,
                                       baton_error_t *error) {
    size_t num_clauses = json_array_size(timestamps);

    size_t i;
    json_t *tp;
    json_array_foreach(timestamps, i, tp) {
        if (!json_is_object(tp)) {
            set_baton_error(error, CAT_INVALID_ARGUMENT,
                            "Invalid timestamp at position %d of %d: "
                            "not a JSON object", i, num_clauses);
            goto error;
        }

        const char *oper = get_timestamp_operator(tp, error);
        if (error->code != 0) goto error;

        if (!oper) {
            oper = SEARCH_OP_EQUALS;
        }

        prepare_tps_search_cb prepare;
        const char *iso_timestamp;

        if (has_created_timestamp(tp)) {
            prepare = prepare_cre;
            iso_timestamp = get_created_timestamp(tp, error);
            if (error->code != 0) goto error;
        }
        else if (has_modified_timestamp(tp)) {
            prepare = prepare_mod;
            iso_timestamp = get_modified_timestamp(tp, error);
            if (error->code != 0) goto error;
        }
        else {
            set_baton_error(error, CAT_INVALID_ARGUMENT,
                            "Invalid timestamp at position %d of %d: "
                            "missing created/modified property",
                            i, num_clauses);
            goto error;
        }

        char *raw_timestamp = parse_timestamp(iso_timestamp, ISO8601_FORMAT);
        if (!raw_timestamp) {
            set_baton_error(error, CAT_INVALID_ARGUMENT,
                            "Invalid timestamp at position %d of %d, "
                            "could not be parsed: '%s'", i, num_clauses,
                            raw_timestamp);
            goto error;
        }

        prepare(query_in, raw_timestamp, oper);
        free(raw_timestamp);
    }

    return query_in;

error:
    return query_in;
}

json_t *add_checksum_json_object(rcComm_t *conn, json_t *object,
                                 baton_error_t *error) {
    char *path = NULL;
    rodsPath_t rods_path;
    json_t *checksum;
    int status;

    if (!json_is_object(object)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid target: not a JSON object");
        goto error;
    }

    path = json_to_path(object, error);
    if (error->code != 0) goto error;

    status = set_rods_path(conn, &rods_path, path);
    if (status < 0) {
        set_baton_error(error, status, "Failed to set iRODS path '%s'", path);
        goto error;
    }

    checksum = list_checksum(conn, &rods_path, error);
    if (error->code != 0) goto error;

    add_checksum(object, checksum, error);
    if (error->code != 0) goto error;

    if (path)                  free(path);
    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);

    return object;

error:
    if (path)                  free(path);
    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);

    return NULL;
}

json_t *add_checksum_json_array(rcComm_t *conn, json_t *array,
                                baton_error_t *error) {
    if (!json_is_array(array)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid target: not a JSON array");
        goto error;
    }

    size_t i;
    json_t *item;
    json_array_foreach(array, i, item) {
        if (represents_data_object(item)) {
            add_checksum_json_object(conn, item, error);
            if (error->code != 0) goto error;
        }
    }

    return array;

error:
    logmsg(ERROR, error->message);
    return NULL;
}

json_t *add_repl_json_object(rcComm_t *conn, json_t *object,
                             baton_error_t *error) {
    char *path = NULL;
    rodsPath_t rods_path;
    json_t *replicates;
    int status;

    if (!json_is_object(object)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid target: not a JSON object");
        goto error;
    }

    path = json_to_path(object, error);
    if (error->code != 0) goto error;

    status = set_rods_path(conn, &rods_path, path);
    if (status < 0) {
        set_baton_error(error, status, "Failed to set iRODS path '%s'", path);
        goto error;
    }

    replicates = list_replicates(conn, &rods_path, error);
    if (error->code != 0) goto error;

    add_replicates(object, replicates, error);
    if (error->code != 0) goto error;

    if (path)                  free(path);
    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);

    return object;

error:
    if (path)                  free(path);
    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);

    return NULL;
}

json_t *add_repl_json_array(rcComm_t *conn, json_t *array,
                            baton_error_t *error) {
    if (!json_is_array(array)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid target: not a JSON array");
        goto error;
    }

    size_t i;
    json_t *item;
    json_array_foreach(array, i, item) {
        if (represents_data_object(item)) {
            add_repl_json_object(conn, item, error);
            if (error->code != 0) goto error;
        }
    }

    return array;

error:
    logmsg(ERROR, error->message);
    return NULL;
}

json_t *add_tps_json_object(rcComm_t *conn, json_t *object,
                            baton_error_t *error) {
    rodsPath_t rods_path;
    char *path             = NULL;
    json_t *raw_timestamps = NULL;
    json_t *timestamps     = NULL;

    if (!json_is_object(object)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid target: not a JSON object");
        goto error;
    }

    path = json_to_path(object, error);
    if (error->code != 0) goto error;

    int status = set_rods_path(conn, &rods_path, path);
    if (status < 0) {
        set_baton_error(error, status, "Failed to set iRODS path '%s'", path);
        goto error;
    }

    raw_timestamps = list_timestamps(conn, &rods_path, error);
    if (error->code != 0) goto error;

    timestamps = json_array();
    if (!timestamps) {
        set_baton_error(error, -1, "Failed to allocate a new JSON array");
        goto error;
    }

    // We report timestamps only on data objects. They exist on
    // collections too, but we don't report them to be consistent with
    // the 'ils' command.
    if (represents_data_object(object)) {
        int base = 10;
        size_t i;
        json_t *item;
        json_array_foreach(raw_timestamps, i, item) {
            const char *repl_str = get_replicate_num(item, error);
            if (error->code != 0) goto error;

            char *endptr;
            int repl_num = strtoul(repl_str, &endptr, base);
            if (*endptr) {
                set_baton_error(error, -1,
                                "Failed to parse replicate number from "
                                "string '%s'", repl_str);
                goto error;
            }

            const char *created = get_created_timestamp(item, error);
            if (error->code != 0) goto error;
            const char *modified = get_modified_timestamp(item, error);
            if (error->code != 0) goto error;

            json_t *iso_created =
                make_timestamp(JSON_CREATED_KEY, created, ISO8601_FORMAT,
                               &repl_num, error);
            if (error->code != 0) goto error;

            json_t *iso_modified =
                make_timestamp(JSON_MODIFIED_KEY, modified, ISO8601_FORMAT,
                               &repl_num, error);
            if (error->code != 0) goto error;

            json_array_append_new(timestamps, iso_created);
            json_array_append_new(timestamps, iso_modified);

            logmsg(DEBUG, "Adding timestamps from replicate %d of '%s'",
                   repl_num, path);
        }
    }

    json_object_set_new(object, JSON_TIMESTAMPS_KEY, timestamps);
    if (error->code != 0) goto error;

    if (path)                  free(path);
    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);

    json_decref(raw_timestamps);

    return object;

error:
    if (path)                  free(path);
    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);
    if (raw_timestamps)        json_decref(raw_timestamps);
    if (timestamps)            json_decref(timestamps);

    return NULL;
}

json_t *add_tps_json_array(rcComm_t *conn, json_t *array,
                           baton_error_t *error) {
    if (!json_is_array(array)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid target: not a JSON array");
        goto error;
    }

    size_t i;
    json_t *item;
    json_array_foreach(array, i, item) {
        if (represents_data_object(item)) {
            add_tps_json_object(conn, item, error);
            if (error->code != 0) goto error;
        }
    }

    return array;

error:
    logmsg(ERROR, error->message);
    return NULL;
}

json_t *add_avus_json_object(rcComm_t *conn, json_t *object,
                             baton_error_t *error) {
    char *path = NULL;
    rodsPath_t rods_path;
    json_t *avus;
    int status;

    if (!json_is_object(object)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid target: not a JSON object");
        goto error;
    }

    path = json_to_path(object, error);
    if (error->code != 0) goto error;

    status = set_rods_path(conn, &rods_path, path);
    if (status < 0) {
        set_baton_error(error, status, "Failed to set iRODS path '%s'", path);
        goto error;
    }

    avus = list_metadata(conn, &rods_path, NULL, error);
    if (error->code != 0) goto error;

    add_metadata(object, avus, error);
    if (error->code != 0) goto error;

    if (path)                  free(path);
    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);

    return object;

error:
    if (path)                  free(path);
    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);

    return NULL;
}

json_t *add_avus_json_array(rcComm_t *conn, json_t *array,
                            baton_error_t *error) {
    if (!json_is_array(array)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid target: not a JSON array");
        goto error;
    }

    size_t i;
    json_t *item;
    json_array_foreach(array, i, item) {
        add_avus_json_object(conn, item, error);
        if (error->code != 0) goto error;
    }

    return array;

error:
    logmsg(ERROR, error->message);
    return NULL;
}

json_t *add_acl_json_object(rcComm_t *conn, json_t *object,
                            baton_error_t *error) {
    char *path = NULL;
    rodsPath_t rods_path;
    json_t *perms;
    int status;

    if (!json_is_object(object)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid target: not a JSON object");
        goto error;
    }

    path = json_to_path(object, error);
    if (error->code != 0) goto error;

    status = set_rods_path(conn, &rods_path, path);
    if (status < 0) {
        set_baton_error(error, status, "Failed to set iRODS path '%s'", path);
        goto error;
    }

    perms = list_permissions(conn, &rods_path, error);
    if (error->code != 0) goto error;

    add_permissions(object, perms, error);
    if (error->code != 0) goto error;

    if (path)                  free(path);
    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);

    return object;

error:
    if (path)                  free(path);
    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);

    return NULL;
}

json_t *add_acl_json_array(rcComm_t *conn, json_t *array,
                           baton_error_t *error) {
    if (!json_is_array(array)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid target: not a JSON array");
        goto error;
    }

    size_t i;
    json_t *item;
    json_array_foreach(array, i, item) {
        add_acl_json_object(conn, item, error);
        if (error->code != 0) goto error;
    }

    return array;

error:
    logmsg(ERROR, error->message);
    return NULL;
}

static int is_zone_hint(const char *path) {
    size_t len = strnlen(path, MAX_STR_LEN);
    int is_zone = 1;

    if (len < 2) {
        is_zone = 0;
    }
    else {
        for (size_t i = 0; i < len; i++) {
            if (strncmp(path, "/", 1) == 0 && i != 0) {
                is_zone = 0;
                break;
            }
            path++;
        }
    }

    return is_zone;
}

static size_t parse_attr_value(int column, const char *label,
                               const char *input, char *output,
                               size_t max_len) {
    size_t size;

    if (maybe_utf8(input, max_len)) {
        size = snprintf(output, max_len, "%s", input);
    }
    else {
        logmsg(WARN,
               "Failed to parse column %d '%s' value '%s' as UTF-8. "
               "Attempting to coerce to UTF-8 assuming it is ISO_8859-1",
               column, label, input);

        size = to_utf8(input, output, max_len);
        if (!maybe_utf8(output, max_len)) {
            size = 0;
            logmsg(ERROR, "Failed to coerce column %d '%s' value '%s' "
                   "to UTF-8", column, label, input);
        }
    }

    return size;
}

json_t *map_access_args(json_t *query, baton_error_t *error) {
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

json_t *revmap_access_result(json_t *acl,  baton_error_t *error) {
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

json_t *revmap_replicate_results(json_t *results, baton_error_t *error) {
    json_t *replicates = json_array();
    if (!replicates) {
        set_baton_error(error, -1, "Failed to allocate a new JSON array");
        goto error;
    }

    size_t num_elts = json_array_size(results);
    for (size_t i = 0; i < num_elts; i++) {
        json_t *result = json_array_get(results, i);
        if (!json_is_object(result)) {
            set_baton_error(error, CAT_INVALID_ARGUMENT,
                            "Invalid replicate result at position %d of %d: ",
                            "not a JSON object", i, num_elts);
            goto error;
        }

        json_t *repl = json_object_get(result, JSON_REPLICATE_NUMBER_KEY);
        size_t n = atol(json_string_value(repl));
        json_object_del(result, JSON_REPLICATE_NUMBER_KEY);
        json_object_set_new(result, JSON_REPLICATE_NUMBER_KEY, json_integer(n));

        json_t *status = json_object_get(result, JSON_REPLICATE_STATUS_KEY);
        json_t *is_valid;
        if (str_equals(json_string_value(status), INVALID_REPLICATE, 1)) {
            is_valid = json_false();
        }
        else if (str_equals(json_string_value(status), VALID_REPLICATE, 1)) {
            is_valid = json_true();
        }
        else {
            set_baton_error(error, CAT_INVALID_ARGUMENT,
                            "Invalid replicate status '%s' at "
                            "position %d of %d", json_string_value(status),
                            i, num_elts);
            goto error;
        }

        json_object_del(result, JSON_REPLICATE_STATUS_KEY);
        json_object_set_new(result, JSON_REPLICATE_STATUS_KEY, is_valid);
    }

    return results;

error:
    if (replicates) json_decref(replicates);
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
