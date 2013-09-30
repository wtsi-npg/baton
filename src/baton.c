/**
 * Copyright (c) 2013 Genome Research Ltd. All rights reserved.
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

char *metadata_op_name(metadata_op op);

void map_mod_args(modAVUMetadataInp_t *out, struct mod_metadata_in *in);

genQueryInp_t *prepare_obj_list(genQueryInp_t *query_input,
                                rodsPath_t *rods_path, char *attr_name);

genQueryInp_t *prepare_col_list(genQueryInp_t *query_input,
                                rodsPath_t *rods_path, char *attr_name);

genQueryInp_t *prepare_obj_search(genQueryInp_t *query_input, char *attr_name,
                                  char *attr_value);

genQueryInp_t *prepare_col_search(genQueryInp_t *query_input, char *attr_name,
                                  char *attr_value);

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

void init_baton_error(struct baton_error *error) {
    assert(error);
    error->message[0] = '\0';
    error->code = 0;
    error->size = 1;
}

void set_baton_error(struct baton_error *error, int code,
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
                               env.rodsZone, RECONN_TIMEOUT, &errmsg);
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

    status = getRodsEnv(env);
    if (status < 0) {
        logmsg(ERROR, BATON_CAT, "Failed to load your iRODS environment");
        goto error;
    }

    rcComm_t *conn = rcConnect(env->rodsHost, env->rodsPort, env->rodsUserName,
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
    if (!dest) return -1;

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

json_t *list_metadata(rcComm_t *conn, rodsPath_t *rods_path, char *attr_name,
                      struct baton_error *error) {
    const char *labels[] = { "attribute", "value", "units" };
    int num_columns = 3;
    int max_rows = 10;
    int columns[num_columns];

    genQueryInp_t *query_input = NULL;
    genQueryOut_t *query_output;
    init_baton_error(error);

    if (rods_path->objState == NOT_EXIST_ST) {
        set_baton_error(error, USER_FILE_DOES_NOT_EXIST,
                        "Path '%s' does not exist "
                        "(or lacks access permission)", rods_path->outPath);
        goto error;
    }

    switch (rods_path->objType) {
        case DATA_OBJ_T:
            logmsg(DEBUG, BATON_CAT, "Indentified '%s' as a data object",
                   rods_path->outPath);
            columns[0] = COL_META_DATA_ATTR_NAME;
            columns[1] = COL_META_DATA_ATTR_VALUE;
            columns[2] = COL_META_DATA_ATTR_UNITS;
            query_input = make_query_input(max_rows, num_columns, columns);
            prepare_obj_list(query_input, rods_path, attr_name);
            break;

        case COLL_OBJ_T:
            logmsg(DEBUG, BATON_CAT, "Indentified '%s' as a collection",
                   rods_path->outPath);
            columns[0] = COL_META_COLL_ATTR_NAME;
            columns[1] = COL_META_COLL_ATTR_VALUE;
            columns[2] = COL_META_COLL_ATTR_UNITS;
            query_input = make_query_input(max_rows, num_columns, columns);
            prepare_col_list(query_input, rods_path, attr_name);
            break;

        default:
            set_baton_error(error, USER_INPUT_PATH_ERR,
                            "Failed to list metadata on '%s' as it is "
                            "neither data object nor collection",
                            rods_path->outPath);
            goto error;
    }

    json_t *results = do_query(conn, query_input, query_output, labels, error);
    free_query_input(query_input);

    if (error->code != 0) goto error;

    return results;

error:
    if (query_input) free_query_input(query_input);
    logmsg(ERROR, BATON_CAT, "Failed to list metadata: error %d %s",
           error->code, error->message);

    return NULL;
}

json_t *search_metadata(rcComm_t *conn, char *attr_name, char *attr_value,
                        struct baton_error *error) {
    int num_columns = 1;
    int max_rows = 10;
    const char *labels[] = { "collection", "data_object" };
    int columns[] = { COL_COLL_NAME, COL_DATA_NAME };

    json_t *results = json_array();
    assert(results);

    genQueryInp_t *query_input = NULL;
    genQueryOut_t *query_output;
    init_baton_error(error);

    logmsg(DEBUG, BATON_CAT, "Searching for collections ...");
    query_input = make_query_input(max_rows, num_columns, columns);
    prepare_col_search(query_input, attr_name, attr_value);

    json_t *collections =
        do_query(conn, query_input, query_output, labels, error);
    if (error->code != 0) goto error;

    logmsg(DEBUG, BATON_CAT, "Found %d matching collections",
           json_array_size(collections));
    json_array_extend(results, collections);
    json_decref(collections);
    free_query_input(query_input);

    logmsg(DEBUG, BATON_CAT, "Searching for data objects ...");
    query_input = make_query_input(max_rows, num_columns + 1, columns);
    prepare_obj_search(query_input, attr_name, attr_value);

    json_t *data_objects =
        do_query(conn, query_input, query_output, labels, error);
    if (error->code != 0) goto error;

    logmsg(DEBUG, BATON_CAT, "Found %d matching data objects",
           json_array_size(data_objects));
    json_array_extend(results, data_objects);
    json_decref(data_objects);
    free_query_input(query_input);

    return results;

error:
    logmsg(ERROR, BATON_CAT, "Failed to search metadata '%s' -> '%s':"
           " error %d %s", attr_name, attr_value, error->code, error->message);

    return NULL;
}

int modify_metadata(rcComm_t *conn, rodsPath_t *rods_path, metadata_op op,
                    char *attr_name, char *attr_value, char *attr_units,
                    struct baton_error *error) {
    int status;
    char *err_name;
    char *err_subname;
    char *type_arg;
    init_baton_error(error);

    if (rods_path->objState == NOT_EXIST_ST) {
        set_baton_error(error, USER_FILE_DOES_NOT_EXIST,
                        "Path '%s' does not exist "
                        "(or lacks access permission)", rods_path->outPath);
        goto error;
    }

    switch (rods_path->objType) {
        case DATA_OBJ_T:
            logmsg(DEBUG, BATON_CAT, "Indentified '%s' as a data object",
                   rods_path->outPath);
            type_arg = "-d";
            break;

        case COLL_OBJ_T:
            logmsg(DEBUG, BATON_CAT, "Indentified '%s' as a collection",
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
    named_args.op = op;
    named_args.type_arg = type_arg;
    named_args.rods_path = rods_path;
    named_args.attr_name = attr_name;
    named_args.attr_value = attr_value;
    named_args.attr_units = attr_units;

    modAVUMetadataInp_t anon_args;
    map_mod_args(&anon_args, &named_args);
    status = rcModAVUMetadata(conn, &anon_args);
    if (status < 0) {
        err_name = rodsErrorName(status, &err_subname);
        set_baton_error(error, status,
                        "Failed to add metadata '%s' -> '%s' to '%s': "
                        "error %d %s %s",
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
                         struct baton_error *error) {
    char *err_name;
    char *err_subname;

    char *attr_name = NULL;
    char *attr_value = NULL;
    char *attr_units = "";

    const char *key;
    json_t *value;
    json_object_foreach(avu, key, value) {
        if ((strcmp(key, JSON_ATTRIBUTE_KEY) == 0)) {
            attr_name = copy_str(json_string_value(value));
        }
        else if ((strcmp(key, JSON_VALUE_KEY) == 0)) {
            attr_value = copy_str(json_string_value(value));
        }
        else if ((strcmp(key, JSON_UNITS_KEY) == 0)) {
            attr_units = copy_str(json_string_value(value));
        }
    }

    return modify_metadata(conn, rods_path, operation,
                           attr_name, attr_value, attr_units, error);
}


genQueryInp_t *make_query_input(int max_rows, int num_columns,
                                const int columns[]) {
    genQueryInp_t *query_input = calloc(1, sizeof (genQueryInp_t));
    assert(query_input);

    int *cols_to_select = calloc(num_columns, sizeof (int));
    for (int i = 0; i < num_columns; i++) {
        cols_to_select[i] = columns[i];
    }

    int *special_select_ops = calloc(num_columns, sizeof (int));
    special_select_ops[0] = 0;

    query_input->selectInp.inx = cols_to_select;
    query_input->selectInp.value = special_select_ops;
    query_input->selectInp.len = num_columns;

    query_input->maxRows = max_rows;
    query_input->continueInx = 0;
    query_input->condInput.len = 0;

    int *query_cond_indices = calloc(MAX_NUM_CONDITIONALS, sizeof (int));
    char **query_cond_values = calloc(MAX_NUM_CONDITIONALS, sizeof (char *));

    query_input->sqlCondInp.inx = query_cond_indices;
    query_input->sqlCondInp.value = query_cond_values;
    query_input->sqlCondInp.len = 0;

    return query_input;
}

void free_query_input(genQueryInp_t *query_input) {
    assert(query_input);

    // Free any strings allocated as query clause values
    for (int i = 0; i < query_input->sqlCondInp.len; i++) {
        free(query_input->sqlCondInp.value[i]);
    }

    free(query_input->selectInp.inx);
    free(query_input->selectInp.value);
    free(query_input->sqlCondInp.inx);
    free(query_input->sqlCondInp.value);
    free(query_input);
}

genQueryInp_t *add_query_conds(genQueryInp_t *query_input, int num_conds,
                               const struct query_cond conds[]) {
    for (int i = 0; i < num_conds; i++) {
        char *op = conds[i].operator;
        char *name = conds[i].value;

        int expr_size = strlen(name) + strlen(op) + 3 + 1;
        char *expr = calloc(expr_size, sizeof (char));
        snprintf(expr, expr_size, "%s '%s'", op, name);

        logmsg(DEBUG, BATON_CAT,
               "Added conditional %d of %d: %s, len %d, op: %s, "
               "total len %d [%s]",
               i, num_conds, name, strlen(name), op, expr_size, expr);

        int current_index = query_input->sqlCondInp.len;
        query_input->sqlCondInp.inx[current_index] = conds[i].column;
        query_input->sqlCondInp.value[current_index] = expr;
        query_input->sqlCondInp.len++;
    }

    return query_input;
}

json_t *do_query(rcComm_t *conn, genQueryInp_t *query_input,
                 genQueryOut_t *query_output, const char *labels[],
                 struct baton_error *error) {
    int status;
    char *err_name;
    char *err_subname;
    int chunk_num = 0;

    json_t *results = json_array();
    assert(results);

    while (chunk_num == 0 || query_output->continueInx > 0) {
        status = rcGenQuery(conn, query_input, &query_output);

        if (status == 0) {
            query_input->continueInx = query_output->continueInx;

            json_t *chunk = make_json_objects(query_output, labels);
            if (!chunk) goto error;

            logmsg(DEBUG, BATON_CAT, "Fetched chunk %d of %d results",
                   chunk_num, json_array_size(chunk));
            chunk_num++;

            status = json_array_extend(results, chunk);
            json_decref(chunk);

            if (status != 0) goto error;
        }
        else if (status == CAT_NO_ROWS_FOUND) {
            logmsg(DEBUG, BATON_CAT, "Query returned no results");
            break;
        }
        else {
            goto error;
        }
    }

    return results;

error:
    if (conn->rError) {
        err_name = rodsErrorName(status, &err_subname);
        set_baton_error(error, status,
                        "Failed get query result: in chunk %d error %d %s %s",
                        chunk_num, status, err_name, err_subname);
        logmsg(ERROR, BATON_CAT, error->message);
        log_rods_errstack(ERROR, BATON_CAT, conn->rError);
    }
    else {
        set_baton_error(error, status,
                        "Failed to convert query result to JSON: "
                        "in chunk %d error %d", chunk_num, status);
        logmsg(ERROR, BATON_CAT, error->message);
    }

    if (results) json_decref(results);

    return NULL;
}

json_t *make_json_objects(genQueryOut_t *query_output, const char *labels[]) {
    json_t *array = json_array();
    assert(array);

    for (int row = 0; row < query_output->rowCnt; row++) {
        json_t *jrow = json_object();
        assert(jrow);

        for (int i = 0; i < query_output->attriCnt; i++) {
            char *result = query_output->sqlResult[i].value;
            result += row * query_output->sqlResult[i].len;

            logmsg(DEBUG, BATON_CAT,
                   "Encoding column %d '%s' value '%s' as JSON",
                   i, labels[i], result);

            json_t *jvalue = json_string(result);
            assert(jvalue);

            json_object_set_new(jrow, labels[i], jvalue);
        }

        int status = json_array_append_new(array, jrow);
        if (status != 0) {
            logmsg(ERROR, BATON_CAT,
                   "Failed to append a new JSON result at row %d of %d",
                   row, query_output->rowCnt);
            goto error;
        }
    }

    return array;

error:
    if (array) json_decref(array);

    return NULL;
}

json_t *rods_path_to_json(rcComm_t *conn, rodsPath_t *rods_path) {
    json_t *result = NULL;

    switch (rods_path->objType) {
        case DATA_OBJ_T:
            logmsg(DEBUG, BATON_CAT, "Indentified '%s' as a data object",
                   rods_path->outPath);
            result = data_object_path_to_json(rods_path->outPath);
            break;

        case COLL_OBJ_T:
            logmsg(DEBUG, BATON_CAT, "Indentified '%s' as a collection",
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

    struct baton_error error;
    json_t *avus = list_metadata(conn, rods_path, NULL, &error);
    if (!avus) goto avu_error;

    int status = json_object_set_new(result, JSON_AVUS_KEY, avus);
    if (status != 0) goto avu_error;

    return result;

avu_error:
    json_decref(result);

error:
    logmsg(ERROR, BATON_CAT, "Failed to covert '%s' to JSON",
           rods_path->outPath);

    return NULL;
}

void map_mod_args(modAVUMetadataInp_t *out, struct mod_metadata_in *in) {
    out->arg0 = metadata_op_name(in->op);
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

char *metadata_op_name(metadata_op op) {
    char *name;

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

genQueryInp_t *prepare_obj_list(genQueryInp_t *query_input,
                                rodsPath_t *rods_path, char *attr_name) {
    char *path = rods_path->outPath;
    size_t len = strlen(path) + 1;

    char *path1 = calloc(len, sizeof (char));
    char *path2 = calloc(len, sizeof (char));
    strncpy(path1, path, len);
    strncpy(path2, path, len);

    char *coll_name = dirname(path1);
    char *data_name = basename(path2);

    struct query_cond cn = { .column = COL_COLL_NAME,
                             .operator = META_SEARCH_EQUALS,
                             .value = coll_name };
    struct query_cond dn = { .column = COL_DATA_NAME,
                             .operator = META_SEARCH_EQUALS,
                             .value = data_name };
    struct query_cond an = { .column = COL_META_DATA_ATTR_NAME,
                             .operator = META_SEARCH_EQUALS,
                             .value = attr_name };

    int num_conds = 2;
    if (attr_name) {
        add_query_conds(query_input, num_conds + 1,
                        (struct query_cond []) { cn, dn, an });
    }
    else {
        add_query_conds(query_input, num_conds,
                        (struct query_cond []) { cn, dn });
    }

    return query_input;
}

genQueryInp_t *prepare_col_list(genQueryInp_t *query_input,
                                rodsPath_t *rods_path, char *attr_name) {
    char *path = rods_path->outPath;
    struct query_cond cn = { .column = COL_COLL_NAME,
                             .operator = META_SEARCH_EQUALS,
                             .value = path };
    struct query_cond an = { .column = COL_META_COLL_ATTR_NAME,
                             .operator = META_SEARCH_EQUALS,
                             .value = attr_name };

    int num_conds = 1;
    if (attr_name) {
        add_query_conds(query_input, num_conds + 1,
                        (struct query_cond []) { cn, an });
    }
    else {
         add_query_conds(query_input, num_conds,
                         (struct query_cond []) { cn });
    }

    return query_input;
}

genQueryInp_t *prepare_obj_search(genQueryInp_t *query_input, char *attr_name,
                                  char *attr_value) {
    struct query_cond an = { .column = COL_META_DATA_ATTR_NAME,
                             .operator = META_SEARCH_EQUALS,
                             .value = attr_name };
    struct query_cond av = { .column = COL_META_DATA_ATTR_VALUE,
                             .operator = META_SEARCH_EQUALS,
                             .value = attr_value };
    int num_conds = 2;
    return add_query_conds(query_input, num_conds,
                           (struct query_cond []) { an, av });
}

genQueryInp_t *prepare_col_search(genQueryInp_t *query_input, char *attr_name,
                                  char *attr_value) {
    struct query_cond an = { .column = COL_META_COLL_ATTR_NAME,
                             .operator = META_SEARCH_EQUALS,
                             .value = attr_name };
    struct query_cond av = { .column = COL_META_COLL_ATTR_VALUE,
                             .operator = META_SEARCH_EQUALS,
                             .value = attr_value };
    int num_conds = 2;
    return add_query_conds(query_input, num_conds,
                           (struct query_cond []) { an, av });
}
