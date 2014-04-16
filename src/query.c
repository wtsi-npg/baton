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
 * @file query.c
 * @author Keith James <kdj@sanger.ac.uk>
 */

#include <assert.h>
#include <errno.h>
#include <libgen.h>
#include <math.h>
#include <string.h>
#include "rodsClient.h"

#include "log.h"
#include "query.h"
#include "utilities.h"

void log_rods_errstack(log_level level, rError_t *error) {
    rErrMsg_t *errmsg;

    int len = error->len;
    for (int i = 0; i < len; i++) {
	    errmsg = error->errMsg[i];
        logmsg(level, "Level %d: %s", i, errmsg->msg);
    }
}

genQueryInp_t *make_query_input(int max_rows, int num_columns,
                                const int columns[]) {
    genQueryInp_t *query_in = calloc(1, sizeof (genQueryInp_t));
    if (!query_in) goto error;

    logmsg(DEBUG, "Preparing a query to select %d columns", num_columns);

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

    int *query_cond_indices = calloc(MAX_NUM_CONDITIONS, sizeof (int));
    if (!query_cond_indices) goto error;

    char **query_cond_values = calloc(MAX_NUM_CONDITIONS, sizeof (char *));
    if (!query_cond_values) goto error;

    query_in->sqlCondInp.inx   = query_cond_indices;
    query_in->sqlCondInp.value = query_cond_values;
    query_in->sqlCondInp.len   = 0;

    return query_in;

error:
    logmsg(ERROR, "Failed to allocate memory: error %d %s",
           errno, strerror(errno));

    return NULL;
}

void free_query_input(genQueryInp_t *query_in) {
    assert(query_in);

    // Free any strings allocated as query clause values
    for (int i = 0; i < query_in->sqlCondInp.len; i++) {
        free(query_in->sqlCondInp.value[i]);
    }

    // Free any key/value pairs, notably zone hints
    for (int i = 0; i < query_in->condInput.len; i++) {
        free(query_in->condInput.keyWord[i]);
        free(query_in->condInput.value[i]);
    }

    if (query_in->condInput.keyWord != NULL) {
        free(query_in->condInput.keyWord);
    }

	if (query_in->condInput.value != NULL) {
        free(query_in->condInput.value);
    }

    free(query_in->selectInp.inx);
    free(query_in->selectInp.value);
    free(query_in->sqlCondInp.inx);
    free(query_in->sqlCondInp.value);
    free(query_in);
}

void free_query_output(genQueryOut_t *query_out) {
    assert(query_out);

    // Free any strings allocated as query results
    for (int i = 0; i < query_out->attriCnt; i++) {
        free(query_out->sqlResult[i].value);
    }

    free(query_out);
}

genQueryInp_t *add_query_conds(genQueryInp_t *query_in, int num_conds,
                               const query_cond_t conds[]) {
    for (int i = 0; i < num_conds; i++) {
        const char *operator = conds[i].operator;
        const char *name     = conds[i].value;

        logmsg(DEBUG, "Adding condition %d of %d: %s %s",
               1, num_conds, name, operator);

        int expr_size = strlen(name) + strlen(operator) + 3 + 1;
        char *expr = calloc(expr_size, sizeof (char));
        if (!expr) goto error;

        snprintf(expr, expr_size, "%s '%s'", operator, name);

        // Find whether this condition has already been added by a
        // previous builder call. If so, adding again would be
        // redundant.
        int redundant = 0;

        for (int j = 0; j < query_in->sqlCondInp.len; j++) {
            int ci = query_in->sqlCondInp.inx[j];
            char *cv = query_in->sqlCondInp.value[j];

            if (ci == conds[i].column && str_equals(cv, expr)) {
                logmsg(DEBUG, "Condition exists in query at position %d, "
                       "not adding: %d '%s'", j, ci, cv);
                redundant = 1;
            }
        }

        if (!redundant) {
            logmsg(DEBUG, "Added condition %d of %d: %s, len %d, op: %s, "
                   "total len %d [%s]",
                   i, num_conds, name, strlen(name), operator, expr_size, expr);

            int current_index = query_in->sqlCondInp.len;
            query_in->sqlCondInp.inx[current_index] = conds[i].column;
            query_in->sqlCondInp.value[current_index] = expr;
            query_in->sqlCondInp.len++;
        }
    }

    return query_in;

error:
    logmsg(ERROR, "Failed to allocate memory: error %d %s",
           errno, strerror(errno));

    return NULL;
}

genQueryInp_t *prepare_obj_list(genQueryInp_t *query_in,
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

    limit_to_newest_repl(query_in);

    free(path1);
    free(path2);

    return query_in;

error:
    logmsg(ERROR, "Failed to allocate memory: error %d %s",
           errno, strerror(errno));

    return NULL;
}

genQueryInp_t *prepare_col_list(genQueryInp_t *query_in,
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

genQueryInp_t *prepare_obj_acl_list(genQueryInp_t *query_in,
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

genQueryInp_t *prepare_col_acl_list(genQueryInp_t *query_in,
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

genQueryInp_t *prepare_obj_tps_list(genQueryInp_t *query_in,
                                    rodsPath_t *rods_path) {
    char *data_id = rods_path->dataId;
    query_cond_t di = { .column   = COL_DATA_ACCESS_DATA_ID,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = data_id };
    int num_conds = 1;
    return add_query_conds(query_in, num_conds, (query_cond_t []) { di });
}

genQueryInp_t *prepare_col_tps_list(genQueryInp_t *query_in,
                                    rodsPath_t *rods_path) {
    char *path = rods_path->outPath;
    query_cond_t cn = { .column   = COL_COLL_NAME,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = path };
    int num_conds = 1;
    return add_query_conds(query_in, num_conds, (query_cond_t []) { cn });
}

genQueryInp_t *prepare_obj_avu_search(genQueryInp_t *query_in,
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
    add_query_conds(query_in, num_conds, (query_cond_t []) { an, av });

    return limit_to_newest_repl(query_in);
}

genQueryInp_t *prepare_col_avu_search(genQueryInp_t *query_in,
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

genQueryInp_t *limit_to_newest_repl(genQueryInp_t *query_in) {
    int col_selector = NEWLY_CREATED_COPY;
    int num_digits = (col_selector == 0) ? 1 : log10(col_selector) + 1;

    char buf[num_digits + 1];
    snprintf(buf, sizeof buf, "%d",col_selector);

    query_cond_t rs = { .column   = COL_D_REPL_STATUS,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = buf };
    int num_conds = 1;
    return add_query_conds(query_in, num_conds, (query_cond_t []) { rs });
}

genQueryInp_t *prepare_obj_acl_search(genQueryInp_t *query_in,
                                      const char *user_id,
                                      const char *access_level) {
    query_cond_t tn = { .column   = COL_DATA_TOKEN_NAMESPACE,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = ACCESS_NAMESPACE };
    query_cond_t ui = { .column   = COL_DATA_ACCESS_USER_ID,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = user_id };
    query_cond_t al = { .column   = COL_DATA_ACCESS_NAME,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = access_level };
    int num_conds = 3;
    return add_query_conds(query_in, num_conds,
                           (query_cond_t []) { tn, ui, al });
}

genQueryInp_t *prepare_col_acl_search(genQueryInp_t *query_in,
                                      const char *user_id,
                                      const char *access_level) {
    query_cond_t tn = { .column   = COL_COLL_TOKEN_NAMESPACE,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = ACCESS_NAMESPACE };
    query_cond_t ui = { .column   = COL_COLL_ACCESS_USER_ID,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = user_id };
    query_cond_t al = { .column   = COL_COLL_ACCESS_NAME,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = access_level };
    int num_conds = 3;
    return add_query_conds(query_in, num_conds,
                           (query_cond_t []) { tn, ui, al });
}

genQueryInp_t *prepare_obj_cre_search(genQueryInp_t *query_in,
                                      const char *raw_timestamp,
                                      const char *operator) {
    query_cond_t ts = { .column   = COL_D_CREATE_TIME,
                        .operator = operator,
                        .value    = raw_timestamp };
    query_cond_t rn = { .column   = COL_DATA_REPL_NUM,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = DEFAULT_REPL_NUM };
    int num_conds = 2;
    return add_query_conds(query_in, num_conds, (query_cond_t []) { ts, rn });
}

genQueryInp_t *prepare_obj_mod_search(genQueryInp_t *query_in,
                                      const char *raw_timestamp,
                                      const char *operator) {
    query_cond_t ts = { .column   = COL_D_MODIFY_TIME,
                        .operator = operator,
                        .value    = raw_timestamp };
    query_cond_t rn = { .column   = COL_DATA_REPL_NUM,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = DEFAULT_REPL_NUM };
    int num_conds = 2;
    return add_query_conds(query_in, num_conds, (query_cond_t []) { ts, rn });
}

genQueryInp_t *prepare_col_cre_search(genQueryInp_t *query_in,
                                      const char *raw_timestamp,
                                      const char *operator) {
    query_cond_t ts = { .column   = COL_COLL_CREATE_TIME,
                        .operator = operator,
                        .value    = raw_timestamp };
    int num_conds = 1;
    return add_query_conds(query_in, num_conds, (query_cond_t []) { ts });
}

genQueryInp_t *prepare_col_mod_search(genQueryInp_t *query_in,
                                      const char *raw_timestamp,
                                      const char *operator) {
    query_cond_t ts = { .column   = COL_COLL_MODIFY_TIME,
                        .operator = operator,
                        .value    = raw_timestamp };
    int num_conds = 1;
    return add_query_conds(query_in, num_conds, (query_cond_t []) { ts });
}

genQueryInp_t *prepare_path_search(genQueryInp_t *query_in,
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
    logmsg(ERROR, "Failed to allocate memory: error %d %s",
           errno, strerror(errno));

    return NULL;
}

genQueryInp_t *prepare_user_search(genQueryInp_t *query_in,
                                   const char *user_name) {
    query_cond_t un = { .column   = COL_USER_NAME,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = user_name };

    int num_conds = 1;
    return add_query_conds(query_in, num_conds,  (query_cond_t []) { un });
}
