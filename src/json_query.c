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
 * @file json_query.c
 * @author Keith James <kdj@sanger.ac.uk>
 */

#include "rodsClient.h"
#include <jansson.h>

#include "json.h"
#include "json_query.h"
#include "query.h"
#include "utilities.h"

void log_json_error(log_level level, const char *category,
                    json_error_t *error) {
    logmsg(level, category, "JSON error: %s, line %d, column %d, position %d",
           error->text, error->line, error->column, error->position);
}

json_t *do_search(rcComm_t *conn, char *zone_name, json_t *query,
                  query_format_in_t *format,
                  prepare_avu_search_cb prepare_avu,
                  prepare_acl_search_cb prepare_acl,
                  prepare_tps_search_cb prepare_tps,
                  baton_error_t *error) {
    // This appears to be the maximum number of rows returned per
    // result chunk
    int max_rows = 10;
    genQueryInp_t *query_in = NULL;
    json_t *items           = NULL;

    query_in = make_query_input(max_rows, format->num_columns, format->columns);

    json_t *root_path = json_object_get(query, JSON_COLLECTION_KEY);
    if (root_path) {
        if (!json_is_string(root_path)) {
            set_baton_error(error, CAT_INVALID_ARGUMENT,
                            "Invalid root path: not a JSON string");
            goto error;
        }
        query_in = prepare_path_search(query_in, json_string_value(root_path));
    }

    // AVUs are mandatory for searches
    json_t *avus = get_avus(query, error);
    if (error->code != 0) goto error;

    query_in = prepare_json_avu_search(query_in, avus, prepare_avu, error);
    if (error->code != 0) goto error;

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

        query_in = prepare_json_tps_search(query_in, tps, prepare_tps, error);
        if (error->code != 0) goto error;
    }

    if (zone_name) {
        logmsg(TRACE, BATON_CAT, "Setting zone to '%s'", zone_name);
        addKeyVal(&query_in->condInput, ZONE_KW, zone_name);
    }

    items = do_query(conn, query_in, format->labels, error);
    if (error->code != 0) goto error;

    free_query_input(query_in);
    logmsg(TRACE, BATON_CAT, "Found %d matching items", json_array_size(items));

    return items;

error:
    if (query_in) free_query_input(query_in);
    if (items)    json_decref(items);

    return NULL;
}

json_t *do_query(rcComm_t *conn, genQueryInp_t *query_in,
                 const char *labels[], baton_error_t *error) {
    int status;
    char *err_name;
    char *err_subname;
    genQueryOut_t *query_out = NULL;
    int chunk_num     = 0;
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

            if (query_out) free_query_output(query_out);
        }
        else if (status == CAT_NO_ROWS_FOUND && chunk_num > 0) {
            // Oddly CAT_NO_ROWS_FOUND is also returned at the end of a
            // batch of chunks; test chunk_num to distinguish catch this
            logmsg(TRACE, BATON_CAT,
                   "Got CAT_NO_ROWS_FOUND at end of results!");
            break;
        }
        else if (status == CAT_NO_ROWS_FOUND) {
            // If this genuinely means no rows have been found, should we
            // free this, or not? Current iRODS leaves this NULL.
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

    if (query_out) free_query_output(query_out);
    if (results)   json_decref(results);

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

genQueryInp_t *prepare_json_acl_search(genQueryInp_t *query_in,
                                       json_t *mapped_acl,
                                       prepare_acl_search_cb prepare,
                                       baton_error_t *error) {
    int num_clauses = json_array_size(mapped_acl);
    if (num_clauses > 1) {
        set_baton_error(error, -1,
                        "Invalid permissions specification "
                        "(contains %d access elements); cannot query "
                        "on more than one access element at a time "
                        "due to limits in the iRODS general query interface",
                        num_clauses);
        goto error;
    }

    for (int i = 0; i < num_clauses; i++) {
        json_t *access = json_array_get(mapped_acl, i);
        if (!json_is_object(access)) {
            set_baton_error(error, CAT_INVALID_ARGUMENT,
                            "Invalid permissions specification at position "
                            "%d of %d: not a JSON object", i, num_clauses);
            goto error;
        }

        const char *owner_id = get_access_owner(access, error);
        if (error->code != 0) goto error;

        const char *access_level = get_access_level(access, error);
        if (error->code != 0) goto error;

        query_in = prepare(query_in, owner_id, access_level);
    }

    return query_in;

error:
    return query_in;
}

genQueryInp_t *prepare_json_avu_search(genQueryInp_t *query_in,
                                       json_t *avus,
                                       prepare_avu_search_cb prepare,
                                       baton_error_t *error) {
    int num_clauses = json_array_size(avus);

    for (int i = 0; i < num_clauses; i++) {
        json_t *avu = json_array_get(avus, i);
        if (!json_is_object(avu)) {
            set_baton_error(error, CAT_INVALID_ARGUMENT,
                            "Invalid AVU at position %d of %d: ",
                            "not a JSON object", i, num_clauses);
            goto error;
        }

        const char *attr_name = get_avu_attribute(avu, error);
        if (error->code != 0) goto error;

        const char *attr_value = get_avu_value(avu, error);
        if (error->code != 0) goto error;

        const char *oper = get_avu_operator(avu, error);
        if (error->code != 0) goto error;

        if (!oper) {
            oper = SEARCH_OP_EQUALS;
        }

        if (str_equals_ignore_case(oper, SEARCH_OP_EQUALS) ||
            str_equals_ignore_case(oper, SEARCH_OP_LIKE)) {
            prepare(query_in, attr_name, attr_value, oper);
        }
        else {
            set_baton_error(error, CAT_INVALID_ARGUMENT,
                            "Invalid operator: expected one of [%s, %s]",
                            SEARCH_OP_EQUALS, SEARCH_OP_LIKE);
        }
     }

    return query_in;

error:
    return query_in;
}

genQueryInp_t *prepare_json_tps_search(genQueryInp_t *query_in,
                                       json_t *timestamps,
                                       prepare_tps_search_cb prepare,
                                       baton_error_t *error) {
    int num_clauses = json_array_size(timestamps);

    for (int i = 0; i < num_clauses; i++) {
        json_t *tp = json_array_get(timestamps, i);
        if (!json_is_object(tp)) {
            set_baton_error(error, CAT_INVALID_ARGUMENT,
                            "Invalid timestamp at position %d of %d: "
                            "not a JSON object", i, num_clauses);
            goto error;
        }

        if (has_created_timestamp(tp)) {
            const char *created = get_created_timestamp(tp, error);
            if (error->code != 0) goto error;

            const char *oper = get_timestamp_operator(tp, error);
            if (error->code != 0) goto error;

            if (!oper) {
                oper = SEARCH_OP_EQUALS;
            }

            char *raw_created = parse_timestamp(created, ISO8601_FORMAT);
            if (!raw_created) {
                set_baton_error(error, CAT_INVALID_ARGUMENT,
                                "Invalid timestamp at position %d of %d, "
                                "could not be parsed: '%s'", i, num_clauses,
                                created);
                goto error;
            }

            prepare(query_in, raw_created, oper);
            free(raw_created);
        }
     }

    return query_in;

error:
    return query_in;
}

json_t *add_tps_json_object(rcComm_t *conn, json_t *object,
                            baton_error_t *error) {
    rodsPath_t rods_path;
    char *path             = NULL;
    json_t *raw_timestamps = NULL;

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

    const char *created = get_created_timestamp(raw_timestamps, error);
    if (error->code != 0) goto error;
    const char *modified = get_modified_timestamp(raw_timestamps, error);
    if (error->code != 0) goto error;

    add_timestamps(object, created, modified, error);
    if (error->code != 0) goto error;

    if (path)                  free(path);
    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);

    json_decref(raw_timestamps);

    return object;

error:
    if (path)                  free(path);
    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);
    if (raw_timestamps)        json_decref(raw_timestamps);

    return NULL;
}

json_t *add_tps_json_array(rcComm_t *conn, json_t *array,
                           baton_error_t *error) {
    if (!json_is_array(array)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid target: not a JSON array");
        goto error;
    }

    for (size_t i = 0; i < json_array_size(array); i++) {
        json_t *item = json_array_get(array, i);
        add_tps_json_object(conn, item, error);
        if (error->code != 0) goto error;
    }

    return array;

error:
    return NULL;
}

json_t *add_avus_json_object(rcComm_t *conn, json_t *object,
                             baton_error_t *error) {
    rodsPath_t rods_path;
    char *path = NULL;

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

    json_t *avus = list_metadata(conn, &rods_path, NULL, error);
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

    for (size_t i = 0; i < json_array_size(array); i++) {
        json_t *item = json_array_get(array, i);
        add_avus_json_object(conn, item, error);
        if (error->code != 0) goto error;
    }

    return array;

error:
    return NULL;
}

json_t *add_acl_json_object(rcComm_t *conn, json_t *object,
                            baton_error_t *error) {
    rodsPath_t rods_path;
    char *path = NULL;

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

    json_t *perms = list_permissions(conn, &rods_path, error);
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

    for (size_t i = 0; i < json_array_size(array); i++) {
        json_t *item = json_array_get(array, i);
        add_acl_json_object(conn, item, error);
        if (error->code != 0) goto error;
    }

    return array;

error:
    return NULL;
}
