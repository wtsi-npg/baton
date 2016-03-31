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
 * @file query.c
 * @author Keith James <kdj@sanger.ac.uk>
 * @author Joshua C. Randall <jcrandall@alum.mit.edu>
 */

#define _GNU_SOURCE
#include <stdio.h>

#include <assert.h>
#include <errno.h>
#include <libgen.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>

#include <jansson.h>

#include "config.h"
#include "error.h"
#include "log.h"
#include "query.h"
#include "utilities.h"

void log_rods_errstack(log_level level, rError_t *error) {
    int len = error->len;
    for (int i = 0; i < len; i++) {
	    rErrMsg_t *errmsg = error->errMsg[i];
        logmsg(level, "Level %d: %s", i, errmsg->msg);
    }
}

genQueryInp_t *make_query_input(size_t max_rows, size_t num_columns,
                                const int columns[]) {
    genQueryInp_t *query_in = calloc(1, sizeof (genQueryInp_t));
    if (!query_in) goto error;

    logmsg(DEBUG, "Preparing a query to select %d columns", num_columns);

    int *cols_to_select = calloc(num_columns, sizeof (int));
    if (!cols_to_select) goto error;

    for (size_t i = 0; i < num_columns; i++) {
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
    size_t sci_len = query_in->sqlCondInp.len;
    for (size_t i = 0; i < sci_len; i++) {
        free(query_in->sqlCondInp.value[i]);
    }

    // Free any key/value pairs, notably zone hints
    size_t ci_len = query_in->condInput.len;
    for (size_t i = 0; i < ci_len; i++) {
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
    size_t num_attr = query_out->attriCnt;
    for (size_t i = 0; i < num_attr; i++) {
        free(query_out->sqlResult[i].value);
    }

    free(query_out);
}

genQueryInp_t *add_query_conds(genQueryInp_t *query_in, size_t num_conds,
                               const query_cond_t conds[]) {
    for (size_t i = 0; i < num_conds; i++) {
        char *column = getAttrNameFromAttrId(conds[i].column);
        const char *operator = conds[i].operator;
        const char *value    = conds[i].value;

        logmsg(DEBUG, "Adding condition %d of %d: %s %s %s",
               1, num_conds, column, operator, value);

        int expr_size = strlen(value) + strlen(operator) + 3 + 1;
        char *expr = calloc(expr_size, sizeof (char));
        if (!expr) goto error;

        if (str_equals_ignore_case(operator, SEARCH_OP_IN, MAX_STR_LEN)) {
            snprintf(expr, expr_size, "%s %s", operator, value);
        } else {
            snprintf(expr, expr_size, "%s '%s'", operator, value);
	}

        logmsg(DEBUG, "Made string %d of %d: op: %s value: %s, len %d, "
               "total len %d [%s]",
               i, num_conds, operator, value, strlen(value), expr_size, expr);

        int current_index = query_in->sqlCondInp.len;
        query_in->sqlCondInp.inx[current_index] = conds[i].column;
        query_in->sqlCondInp.value[current_index] = expr;
        query_in->sqlCondInp.len++;
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

    size_t num_conds = 2;
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

    size_t num_conds = 1;
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

    size_t num_conds = 2;
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
    size_t num_conds = 2;
    return add_query_conds(query_in, num_conds, (query_cond_t []) { cn, tn });
}

genQueryInp_t *prepare_obj_repl_list(genQueryInp_t *query_in,
                                     rodsPath_t *rods_path) {
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
    size_t num_conds = 2;
    add_query_conds(query_in, num_conds, (query_cond_t []) { cn, dn });

    free(path1);
    free(path2);

    return query_in;

error:
    logmsg(ERROR, "Failed to allocate memory: error %d %s",
           errno, strerror(errno));

    return NULL;
}

genQueryInp_t *prepare_col_tps_list(genQueryInp_t *query_in,
                                    rodsPath_t *rods_path) {
    char *path = rods_path->outPath;
    query_cond_t cn = { .column   = COL_COLL_NAME,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = path };
    size_t num_conds = 1;
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
    size_t num_conds = 2;
    return add_query_conds(query_in, num_conds, (query_cond_t []) { an, av });
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
    size_t num_conds = 2;
    return add_query_conds(query_in, num_conds, (query_cond_t []) { an, av });
}

genQueryInp_t *limit_to_newest_repl(genQueryInp_t *query_in) {
    int col_selector = NEWLY_CREATED_COPY;
    int num_digits = (col_selector == 0) ? 1 : log10(col_selector) + 1;

    char buf[num_digits + 1];
    snprintf(buf, sizeof buf, "%d", col_selector);

    query_cond_t rs = { .column   = COL_D_REPL_STATUS,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = buf };
    size_t num_conds = 1;
    return add_query_conds(query_in, num_conds, (query_cond_t []) { rs });
}

genQueryInp_t *prepare_obj_acl_search(genQueryInp_t *query_in,
                                      const char *user,
                                      const char *access_level) {
    query_cond_t un = { .column   = COL_USER_NAME,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = user };
    query_cond_t al = { .column   = COL_DATA_ACCESS_NAME,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = access_level };
    size_t num_conds = 2;
    return add_query_conds(query_in, num_conds,
                           (query_cond_t []) { un, al });
}

genQueryInp_t *prepare_col_acl_search(genQueryInp_t *query_in,
                                      const char *user,
                                      const char *access_level) {
    query_cond_t un = { .column   = COL_USER_NAME,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = user };
    query_cond_t al = { .column   = COL_COLL_ACCESS_NAME,
                        .operator = SEARCH_OP_EQUALS,
                        .value    = access_level };
    size_t num_conds = 2;
    return add_query_conds(query_in, num_conds,
                           (query_cond_t []) { un, al });
}

genQueryInp_t *prepare_obj_cre_search(genQueryInp_t *query_in,
                                      const char *raw_timestamp,
                                      const char *operator) {
    query_cond_t ts = { .column   = COL_D_CREATE_TIME,
                        .operator = operator,
                        .value    = raw_timestamp };
    size_t num_conds = 1;
    return add_query_conds(query_in, num_conds, (query_cond_t []) { ts });
}

genQueryInp_t *prepare_obj_mod_search(genQueryInp_t *query_in,
                                      const char *raw_timestamp,
                                      const char *operator) {
    query_cond_t ts = { .column   = COL_D_MODIFY_TIME,
                        .operator = operator,
                        .value    = raw_timestamp };
    size_t num_conds = 1;
    return add_query_conds(query_in, num_conds, (query_cond_t []) { ts });
}

genQueryInp_t *prepare_col_cre_search(genQueryInp_t *query_in,
                                      const char *raw_timestamp,
                                      const char *operator) {
    query_cond_t ts = { .column   = COL_COLL_CREATE_TIME,
                        .operator = operator,
                        .value    = raw_timestamp };
    size_t num_conds = 1;
    return add_query_conds(query_in, num_conds, (query_cond_t []) { ts });
}

genQueryInp_t *prepare_col_mod_search(genQueryInp_t *query_in,
                                      const char *raw_timestamp,
                                      const char *operator) {
    query_cond_t ts = { .column   = COL_COLL_MODIFY_TIME,
                        .operator = operator,
                        .value    = raw_timestamp };
    size_t num_conds = 1;
    return add_query_conds(query_in, num_conds, (query_cond_t []) { ts });
}

genQueryInp_t *prepare_path_search(genQueryInp_t *query_in,
                                   const char *root_path) {
    size_t len = strnlen(root_path, MAX_STR_LEN);
    char *path;

    if (len > 0) {
        // Absolute path
        if (str_starts_with(root_path, "/", 1)) {
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

        logmsg(DEBUG, "Adding search clause path LIKE '%s'", path);

        size_t num_conds = 1;
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

    size_t num_conds = 1;
    return add_query_conds(query_in, num_conds,  (query_cond_t []) { un });
}

specificQueryInp_t *prepare_specific_query(specificQueryInp_t *squery_in,
                                           const char *sql, json_t *args) {

    size_t index;
    json_t *value;

    squery_in->maxRows = SEARCH_MAX_ROWS;
    squery_in->continueInx = 0;
    squery_in->sql = (char *)sql;

    json_array_foreach(args, index, value) {
      if (json_is_string(value)) {
          squery_in->args[index] = strdup(json_string_value(value));
      } else {
          goto error;
      }
    }

    return squery_in;

error:
    logmsg(ERROR, "Failed to parse JSON specific query args: %s",
           args);
    return squery_in;
}

void free_squery_input(specificQueryInp_t *squery_in) {
    unsigned int i;
    assert(squery_in);

    for (i=0; i<10; i++) {
        if(squery_in->args[i] != NULL) {
            free(squery_in->args[i]);
        }
    }
    // do not free squery_in->sql as it belongs to target json
    free(squery_in);
}

query_format_in_t *make_query_format_from_sql(const char *sql) {
    query_format_in_t *format = NULL;
    unsigned int reti;
    char remsg[MAX_ERROR_MESSAGE_LEN];

    const char *select_list_capt_re_str =
      "^.*?select[[:space:]]+"
      "(distinct|all[[:space:]]+)?(.*?[^[:space:]])[[:space:]]+"
      "from[[:space:]].*$";
    const unsigned int select_list_capt_idx = 2;
    regex_t select_list_capt_re;
    regmatch_t select_list_pmatch[select_list_capt_idx+1];

    const char *trim_whitespace_capt_re_str =
      "^[[:space:]]*(.*?[^[:space:]])[[:space:]]*$";
    const unsigned int trim_whitespace_capt_idx = 1;
    regex_t trim_whitespace_capt_re;
    regmatch_t trim_whitespace_pmatch[trim_whitespace_capt_idx+1];

    const char *as_column_name_capt_re_str =
      "^.*[[:space:]]+as[[:space:]]+(.*?[^[:space:]])[[:space:]]*$";
    const unsigned int as_column_name_capt_idx = 1;
    regex_t as_column_name_capt_re;
    regmatch_t as_column_name_pmatch[as_column_name_capt_idx+1];

    char *select_list, *select_list_tokenize;
    char *column, *column_trim, *column_name;
    unsigned int i;

    reti = regcomp(&select_list_capt_re, select_list_capt_re_str,
                   REG_EXTENDED | REG_ICASE);
    if (reti != 0) {
        regerror(reti, &select_list_capt_re, remsg, MAX_ERROR_MESSAGE_LEN);
        logmsg(ERROR, "Could not compile regex: '%s': %s",
               select_list_capt_re_str, remsg);
        goto error;
    }

    reti = regcomp(&trim_whitespace_capt_re, trim_whitespace_capt_re_str,
                   REG_EXTENDED | REG_ICASE);
    if (reti != 0) {
        regerror(reti, &trim_whitespace_capt_re, remsg, MAX_ERROR_MESSAGE_LEN);
        logmsg(ERROR, "Could not compile regex: '%s': %s",
               trim_whitespace_capt_re_str, remsg);
        goto error;
    }

    reti = regcomp(&as_column_name_capt_re, as_column_name_capt_re_str,
                   REG_EXTENDED | REG_ICASE);
    if (reti != 0) {
        regerror(reti, &as_column_name_capt_re, remsg, MAX_ERROR_MESSAGE_LEN);
        logmsg(ERROR, "Could not compile regex: '%s': %s",
               as_column_name_capt_re_str, remsg);
        goto error;
    }

    format = calloc(1, sizeof(query_format_in_t));
    if (!format) goto error;

    // extract select list from SQL query
    logmsg(DEBUG, "Extracting select column labels from SQL query: '%s'", sql);
    reti = regexec(&select_list_capt_re, sql, select_list_capt_idx + 1,
                   select_list_pmatch, 0);
    if (reti == 0) {
        logmsg(DEBUG, "Extracting select_list from positions %u to %u",
               select_list_pmatch[select_list_capt_idx].rm_so,
               select_list_pmatch[select_list_capt_idx].rm_eo);

        select_list =
          strndup(sql + select_list_pmatch[select_list_capt_idx].rm_so,
                  select_list_pmatch[select_list_capt_idx].rm_eo -
                  select_list_pmatch[select_list_capt_idx].rm_so + 1);
        logmsg(DEBUG, "Extracted select_list: '%s'", select_list);
    } else if (reti == REG_NOMATCH) {
        logmsg(ERROR, "Regex '%s' did not match SQL: '%s'",
               select_list_capt_re_str, sql);
        goto error;
    } else {
        logmsg(ERROR, "Regex match '%s' failed parsing SQL: '%s'",
               select_list_capt_re_str, sql);
        goto error;
    }
    assert(select_list);

    // don't clobber select_list pointer as we need to free it later
    select_list_tokenize = select_list;

    // parse select_list_tokenize column by column
    for (i = 0; select_list_tokenize != NULL; i++) {
        // get next column specification from select_list_tokenize
        column = strsep(&select_list_tokenize, ",");
        logmsg(DEBUG, "Parsing column %d: strsep found column '%s' "
               "with remaining select_list_tokenize '%s'",
               i, column, select_list_tokenize);

        // trim whitespace from column
        reti = regexec(&trim_whitespace_capt_re, column,
                       trim_whitespace_capt_idx + 1,
                       trim_whitespace_pmatch, 0);
        if (reti == 0) {
            // point column_trim at first non-whitespace character
            column_trim = column +
              trim_whitespace_pmatch[trim_whitespace_capt_idx].rm_so;
            // truncate at character after last non-whitespace character
            column_trim[
                trim_whitespace_pmatch[trim_whitespace_capt_idx].rm_eo -
                trim_whitespace_pmatch[trim_whitespace_capt_idx].rm_so] = '\0';
            logmsg(DEBUG, "Trimmed whitespace from column: '%s'", column_trim);
        } else if (reti == REG_NOMATCH) {
            logmsg(ERROR, "Attempting to trim whitespace regex '%s' "
                   "did not match column: '%s'",
                   trim_whitespace_capt_re_str, column);
            goto error_recoverable;
        } else {
            logmsg(ERROR, "Regex match '%s' failed parsing column: '%s'",
                   trim_whitespace_capt_re_str, column);
            goto error_recoverable;
        }
        assert(column_trim);

        // check for 'AS column_name' in column_trim
        reti = regexec(&as_column_name_capt_re, column_trim,
                       as_column_name_capt_idx + 1,
                       as_column_name_pmatch, 0);
        if (reti == 0) {
            // have 'AS column_name' - use it as the column_name
            column_name =
              strndup(column_trim +
                      as_column_name_pmatch[as_column_name_capt_idx].rm_so,
                      as_column_name_pmatch[as_column_name_capt_idx].rm_eo -
                      as_column_name_pmatch[as_column_name_capt_idx].rm_so + 1);
            if (!column_name) {
                format->num_columns = i;
                logmsg(ERROR, "Could not allocate memory for column_name "
                       "from column: '%s'", column_trim);
                goto error_recoverable;
            }
            logmsg(DEBUG, "Found 'AS <column_name>': '%s'", column_name);
        } else if (reti == REG_NOMATCH) {
            column_name = strdup(column_trim);
            if (!column_name) {
                format->num_columns = i;
                logmsg(ERROR, "Could not allocate memory for column: '%s'",
                       column_trim);
                goto error_recoverable;
            }
            logmsg(DEBUG, "Did not find 'AS <column_name>', "
                   "using whole column specifier as column_name: '%s'",
                   column_name);
        } else {
            logmsg(ERROR, "Regex match '%s' failed parsing column: '%s'",
                   as_column_name_capt_re_str, column);
            goto error_recoverable;
        }
        assert(column_name);

        logmsg(DEBUG, "Found column %d: '%s'", i, column_name);
        format->labels[i] = column_name;
    }
    format->num_columns = i;

    free(select_list);
    regfree(&select_list_capt_re);
    regfree(&trim_whitespace_capt_re);
    regfree(&as_column_name_capt_re);
    return format;

error_recoverable:
    logmsg(ERROR, "Could not parse select columns from SQL "
           "into query format: '%s'", sql);
    // create generic columns labels as a backup plan
    format->num_columns = MAX_NUM_COLUMNS;
    for (i=0; i<(format->num_columns-1); i++) {
        if (format->labels[i] == NULL) {
            reti = asprintf((char**)&format->labels[i], "col%d", i);
        }
    }
    return format;

error:
    logmsg(ERROR, "Could not process SQL: '%s'", sql);
    return NULL;
}

const char *irods_get_sql_for_specific_alias(rcComm_t *conn,
                                             const char *alias) {
    const char *sql;

    specificQueryInp_t *sql_alias_squery_in;
    genQueryOut_t *query_out = NULL;

    char *err_name;
    char *err_subname;
    int status;

    sql_alias_squery_in = calloc(1, sizeof (specificQueryInp_t));
    if (!sql_alias_squery_in) goto error;

    sql_alias_squery_in->maxRows = SEARCH_MAX_ROWS;
    sql_alias_squery_in->continueInx = 0;
    sql_alias_squery_in->sql = "findQueryByAlias";
    sql_alias_squery_in->args[0] = (char *)alias;

    status = rcSpecificQuery(conn, sql_alias_squery_in, &query_out);
    if (status == 0) {
      logmsg(DEBUG, "Successfully fetched SQL for alias: '%s'", alias);
    }
    else if (status == CAT_NO_ROWS_FOUND) {
      logmsg(ERROR, "Query for specific alias '%s' returned no results", alias);
      goto error;
    }
    else {
      err_name = rodsErrorName(status, &err_subname);
      logmsg(ERROR, "Failed to fetch SQL for specific query alias: '%s' "
             "error %d %s %s",
             alias, status, err_name, err_subname);
      goto error;
    }

    size_t num_rows = (size_t) query_out->rowCnt;
    if (num_rows != 1) {
        logmsg(ERROR, "Unexpectedly found %d rows of results "
               "querying specific alias: '%s'", num_rows, alias);
        goto error;
    }

    size_t num_attr = query_out->attriCnt;
    if (num_attr != 2) {
        logmsg(ERROR, "Unexpectedly found %d attributes querying "
               "specific alias: '%s'", num_attr, alias);
        goto error;
    }

    char *found_alias = query_out->sqlResult[0].value;
    if (strcmp(found_alias, alias) != 0) {
        logmsg(ERROR, "Query for specific alias returned non-matching result. "
               "query alias: '%s', result alias: '%s'", alias, found_alias);
        goto error;
    }

    sql = query_out->sqlResult[1].value;
    logmsg(TRACE, "Found SQL for specific alias '%s': '%s'", alias, sql);

    return sql;
error:
    logmsg(ERROR, "Could not find SQL for specific alias: '%s'", alias);
    return NULL;
}

query_format_in_t *prepare_specific_labels(rcComm_t *conn,
                                           const char *sql_or_alias) {
    regex_t select_s_re;
    unsigned int reti;
    char remsg[MAX_ERROR_MESSAGE_LEN];

    const char *sql;
    const char *select_s_re_str ="^select[[:space:]]";
    query_format_in_t *format;

    // does sql_or_alias begin with a SQL SELECT statement?
    reti = regcomp(&select_s_re, select_s_re_str, REG_EXTENDED | REG_ICASE);
    if (reti != 0) {
        regerror(reti, &select_s_re, remsg, MAX_ERROR_MESSAGE_LEN);
        logmsg(ERROR, "Could not compile regex: '%s': %s", select_s_re_str,
               remsg);
        goto error;
    }
    reti = regexec(&select_s_re, sql_or_alias, 0, NULL, 0);
    if (reti == 0) {
        // yes, sql_or_alias does contain SELECT - we already have SQL
        sql = sql_or_alias;
        logmsg(DEBUG, "Already have SQL specific query: '%s'", sql);
    } else if (reti == REG_NOMATCH) {
        // no SELECT found in sql_or_alias we must have an alias (or a
        // bad query, but try to look up the alias anyway)
        sql = irods_get_sql_for_specific_alias(conn, sql_or_alias);
        if (sql == NULL) {
            goto error;
        }
        logmsg(DEBUG, "Got SQL for specific alias '%s': '%s'",
               sql_or_alias, sql);
    } else {
        logmsg(ERROR, "Regex match failed parsing SQL: '%s'", sql_or_alias);
        goto error;
    }
    regfree(&select_s_re);
    assert(sql);

    format = make_query_format_from_sql(sql);

    return format;

error:
    logmsg(ERROR, "Failed to prepare labels for specific query: '%s'",
           sql_or_alias);
    return NULL;
}

void free_specific_labels(query_format_in_t *format) {
    unsigned int i;
    assert(format);

    for (i=0; i<(format->num_columns-1); i++) {
      free((void *)(format->labels[i]));
    }
    free(format);
}
