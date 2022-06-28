/**
 * Copyright (C) 2013, 2014, 2015, 2016, 2017, 2019, 2021 Genome
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
 * @file list.c
 * @author Keith James <kdj@sanger.ac.uk>
 */

#include "list.h"
#include "read.h"

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
              .columns     = { COL_COLL_NAME, COL_DATA_NAME,
                               COL_DATA_SIZE },
              .labels      = { JSON_COLLECTION_KEY, JSON_DATA_OBJECT_KEY,
                               JSON_SIZE_KEY } };
    }
    else {
        obj_format = &(query_format_in_t)
            { .num_columns = 2,
              .columns     = { COL_COLL_NAME, COL_DATA_NAME },
              .labels      = { JSON_COLLECTION_KEY, JSON_DATA_OBJECT_KEY } };
    }

    query_in = make_query_input(SEARCH_MAX_ROWS, obj_format->num_columns,
                                obj_format->columns);
    query_in = prepare_obj_list(query_in, rods_path, NULL);
    query_in = limit_to_good_repl(query_in);

    results = do_query(conn, query_in, obj_format->labels, error);
    if (error->code != 0) goto error;

    if (json_array_size(results) != 1) {
        set_baton_error(error, -1, "Expected 1 data object result but "
                        "found %d. This occurs when the object replicates "
                        "have different sizes in the iRODS database.",
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
    json_t *results = NULL;

    int query_flags = DATA_QUERY_FIRST_FG;
    collHandle_t coll_handle;
    collEnt_t coll_entry;

    int status = rclOpenCollection(conn, rods_path->outPath, query_flags,
                                   &coll_handle);
    if (status < 0) {
        char *err_subname;
        const char *err_name = rodsErrorName(status, &err_subname);
        set_baton_error(error, status,
                        "Failed to open collection: '%s' error %d %s",
                        rods_path->outPath, status, err_name);
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
                                                  coll_entry.dataName,
                                                  error);
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

json_t *list_checksum(rcComm_t *conn, rodsPath_t *rods_path,
                      baton_error_t *error) {
    genQueryInp_t *query_in = NULL;
    json_t *results         = NULL;

    init_baton_error(error);

    if (rods_path->objState == NOT_EXIST_ST) {
        set_baton_error(error, USER_FILE_DOES_NOT_EXIST,
                        "Path '%s' does not exist "
                        "(or lacks access permission)", rods_path->outPath);
        goto error;
    }

    if (rods_path->objType != DATA_OBJ_T) {
        set_baton_error(error, USER_INPUT_PATH_ERR,
                        "Failed to get the checksum of '%s' as it is "
                        "not a data object",  rods_path->outPath);
        goto error;
    }

    query_format_in_t obj_format =
        { .num_columns = 3,
          .columns     = { COL_COLL_NAME, COL_DATA_NAME,
                           COL_D_DATA_CHECKSUM },
          .labels      = { JSON_COLLECTION_KEY, JSON_DATA_OBJECT_KEY,
                           JSON_CHECKSUM_KEY } };

    query_in = make_query_input(SEARCH_MAX_ROWS, obj_format.num_columns,
                                obj_format.columns);
    query_in = prepare_obj_list(query_in, rods_path, NULL);
    query_in = limit_to_good_repl(query_in);

    results = do_query(conn, query_in, obj_format.labels, error);
    if (error->code != 0) goto error;

    if (json_array_size(results) != 1) {
        set_baton_error(error, -1, "Expected 1 data object result but "
                        "found %d. This occurs when the object replicates "
                        "have different checksum values in the iRODS database",
                        json_array_size(results));
        goto error;
    }

    json_t *obj = json_array_get(results, 0);
    json_t *checksum = json_incref(json_object_get(obj, JSON_CHECKSUM_KEY));

    free_query_input(query_in);
    if (results) json_decref(results);

    return checksum;

error:
    if (query_in) free_query_input(query_in);
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
            if (flags & PRINT_CHECKSUM) {
                result = add_checksum_json_object(conn, result, error);
                if (error->code != 0) goto error;
            }
            if (flags & PRINT_TIMESTAMP) {
                result = add_tps_json_object(conn, result, error);
                if (error->code != 0) goto error;
            }
            if (flags & PRINT_REPLICATE) {
              result = add_repl_json_object(conn, result, error);
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
                if (flags & PRINT_CHECKSUM) {
                    contents = add_checksum_json_array(conn, contents, error);
                    if (error->code != 0) goto error;
                }
                if (flags & PRINT_TIMESTAMP) {
                    contents = add_tps_json_array(conn, contents, error);
                    if (error->code != 0) goto error;
                }
                if (flags & PRINT_REPLICATE) {
                    contents = add_repl_json_array(conn, contents, error);
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

json_t *list_permissions(rcComm_t *conn, rodsPath_t *rods_path,
                         baton_error_t *error) {
    genQueryInp_t *query_in = NULL;
    json_t *results         = NULL;

    // There are two options for showing permissions in iRODS; either
    // with groups expanded into their constituent users, or without.
    //
    // As of iRODS 4.2.5, ils uses the former for collections and the
    // latter for data objects.
    //
    // i.e. a data object readable by public would show as
    //
    //     public#testZone:read
    //
    // while a collections readable by public would show as
    //
    //     irods#testZone:own irods#testZone:read object
    //     john#testZone:read object alice#testZone:read object
    //
    // where irods, john and alice are the members of "public".

    // This reports groups unexpanded (substuting COL_DATA_USER_NAME
    // for COL_USER_NAME reports constituent users):
    query_format_in_t obj_format =
        { .num_columns = 3,
          .columns     = { COL_USER_NAME, COL_USER_ZONE,
                           COL_DATA_ACCESS_NAME },
          .labels      = { JSON_OWNER_KEY, JSON_ZONE_KEY, JSON_LEVEL_KEY } };

    // This reports constituent users (substituing COL_USER_NAME
    // for COL_COLL_USER_NAME causes incorrect reporting)
    query_format_in_t col_format =
        { .num_columns = 3,
          .columns     = { COL_COLL_USER_NAME, COL_COLL_USER_ZONE,
                           COL_COLL_ACCESS_NAME },
          .labels      = { JSON_OWNER_KEY, JSON_ZONE_KEY, JSON_LEVEL_KEY } };

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

json_t *list_replicates(rcComm_t *conn, rodsPath_t *rods_path,
                        baton_error_t *error) {
    genQueryInp_t *query_in = NULL;
    json_t *results         = NULL;

#if IRODS_VERSION_INTEGER && IRODS_VERSION_INTEGER >= 4001008
    query_format_in_t obj_format =
        { .num_columns = 5,
          .columns     = { COL_D_REPL_STATUS,
                           COL_DATA_REPL_NUM,
                           COL_D_DATA_CHECKSUM,
                           COL_COLL_NAME, COL_D_RESC_HIER },
          .labels      = { JSON_REPLICATE_STATUS_KEY,
                           JSON_REPLICATE_NUMBER_KEY,
                           JSON_CHECKSUM_KEY,
                           JSON_COLLECTION_KEY, JSON_RESOURCE_HIER_KEY } };
#else
    query_format_in_t obj_format =
        { .num_columns = 5,
          .columns     = { COL_D_REPL_STATUS,
                           COL_DATA_REPL_NUM,
                           COL_D_DATA_CHECKSUM,
                           COL_D_RESC_NAME, COL_R_LOC },
          .labels      = { JSON_REPLICATE_STATUS_KEY,
                           JSON_REPLICATE_NUMBER_KEY,
                           JSON_CHECKSUM_KEY,
                           JSON_RESOURCE_KEY, JSON_LOCATION_KEY } };
#endif

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
            query_in = prepare_obj_repl_list(query_in, rods_path);
            break;

        case COLL_OBJ_T:
            logmsg(TRACE, "Identified '%s' as a collection",
                   rods_path->outPath);
            set_baton_error(error, USER_INPUT_PATH_ERR,
                            "Failed to list replicates of '%s' as it is "
                            "a collection", rods_path->outPath);
            break;

        default:
            set_baton_error(error, USER_INPUT_PATH_ERR,
                            "Failed to list replicates of '%s' as it is "
                            "neither data object nor collection",
                            rods_path->outPath);
            goto error;
    }

    addKeyVal(&query_in->condInput, ZONE_KW, rods_path->outPath);
    logmsg(DEBUG, "Using zone hint '%s'", rods_path->outPath);
    results = do_query(conn, query_in, obj_format.labels, error);
    if (error->code != 0) goto error;

    json_t *mapped = revmap_replicate_results(conn, results, error);
    if (error->code != 0) goto error;

    logmsg(DEBUG, "Obtained replicates of '%s'", rods_path->outPath);
    free_query_input(query_in);
    json_decref(results);

    return mapped;

error:
    logmsg(ERROR, "Failed to list replicates of '%s': error %d %s",
           rods_path->outPath, error->code, error->message);

    if (query_in) free_query_input(query_in);
    if (results)  json_decref(results);

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
            query_in = prepare_obj_list(query_in, rods_path, NULL);
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
