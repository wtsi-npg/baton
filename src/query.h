/**
 * Copyright (C) 2013, 2014, 2015, 2016 Genome Research Ltd. All
 * rights reserved.
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
 * @file query.h
 * @author Keith James <kdj@sanger.ac.uk>
 * @author Joshua C. Randall <jcrandall@alum.mit.edu>
*/

#ifndef _BATON_QUERY_H
#define _BATON_QUERY_H

#include <jansson.h>
#include <rodsClient.h>

#include "config.h"
#include "log.h"
#include "utilities.h"

#define MAX_NUM_COLUMNS     128
#define MAX_NUM_CONDITIONS   32

#define SEARCH_MAX_ROWS      10

#define SEARCH_OP_EQUALS   "="
#define SEARCH_OP_LIKE     "like"
#define SEARCH_OP_NOT_LIKE "not like"
#define SEARCH_OP_IN       "in"
#define SEARCH_OP_STR_GT   ">"
#define SEARCH_OP_STR_LT   "<"
#define SEARCH_OP_NUM_GT   "n>"
#define SEARCH_OP_NUM_LT   "n<"
#define SEARCH_OP_STR_GE   ">="
#define SEARCH_OP_STR_LE   "<="
#define SEARCH_OP_NUM_GE   "n>="
#define SEARCH_OP_NUM_LE   "n<="

#define DEFAULT_REPL_NUM "0"

#define ACCESS_NAMESPACE   "access_type"
#define ACCESS_LEVEL_NULL  "null"
#define ACCESS_LEVEL_OWN   "own"
#define ACCESS_LEVEL_READ  "read"
#define ACCESS_LEVEL_WRITE "write"

typedef struct query_format_in {
    /** Return data for the latest replicate only */
    unsigned int latest;
    /** The number of columns to return */
    unsigned num_columns;
    /** The ICAT columns to return */
    const int columns[MAX_NUM_COLUMNS];
    /** The labels to use for the returned column values */
    const char *labels[MAX_NUM_COLUMNS];
} query_format_in_t;

typedef struct query_cond {
    /** The ICAT column to match e.g. COL_META_DATA_ATTR_NAME */
    int column;
    /** The operator to use e.g. "=", "<", ">" */
    const char *operator;
    /** The value to match */
    const char *value;
} query_cond_t;

typedef genQueryInp_t *(*prepare_avu_search_cb) (genQueryInp_t *query_in,
                                                 const char *attr_name,
                                                 const char *attr_value,
                                                 const char *operator);

typedef genQueryInp_t *(*prepare_acl_search_cb) (genQueryInp_t *query_in,
                                                 const char *user_id,
                                                 const char *access_level);

typedef genQueryInp_t *(*prepare_tps_search_cb) (genQueryInp_t *query_in,
                                                 const char *raw_timestamp,
                                                 const char *operator);

typedef specificQueryInp_t *(*prepare_specific_query_cb) (specificQueryInp_t *squery_in,
                                                          const char *sql,
                                                          json_t *args);

typedef query_format_in_t *(*prepare_specific_labels_cb) (rcComm_t *conn,
                                                          const char *sql);

/**
 * Log the current iRODS error stack through the underlying logging
 * mechanism.
 *
 * @param[in] level     The logging level.
 * @param[in] error     The iRODS error state.
 */
void log_rods_errstack(log_level level, rError_t *error);

/**
 * Allocate a new iRODS generic query (see rodsGenQuery.h).
 *
 * @param[in] max_rows     Maximum number of rows to return.
 * @param[in] num_columns  The number of columns to select.
 * @param[in] columns      The columns to select.
 *
 * @return A pointer to a new genQueryInp_t which must be freed using
 * @ref free_query_input
 */
genQueryInp_t *make_query_input(size_t max_rows, size_t num_columns,
                                const int columns[]);
/**
 * Free memory used by an iRODS generic query (see rodsGenQuery.h).
 *
 * @param[in] query_in       The query to free.
 *
 * @ref make_query_input
 */
void free_query_input(genQueryInp_t *query_in);

/**
 * Free memory used by an iRODS generic query result (see
 * rodsGenQuery.h).
 *
 * @param[in] query_out       The query result to free.
 */
void free_query_output(genQueryOut_t *query_out);

/**
 * Append a new array of conditions to an existing query.
 *
 * @param[in] query_in       The query to free.
 * @param[in] num_conds      The number of conditions to append.
 * @param[in] conds          An array of conditions to append.
 *
 * @return The modified query.
 */
genQueryInp_t *add_query_conds(genQueryInp_t *query_in, size_t num_conds,
                               const query_cond_t conds[]);

/**
 * Add a clause to a query to list AVUs on a data object, optionally
 * restricting results to a specific attribute.
 *
 * @param[out] query_in      The query to update.
 * @param[in]  rods_path     The target data object.
 * @param[in]  attr_name     An attribute name.
 *
 * @return The modified query.
 */
genQueryInp_t *prepare_obj_list(genQueryInp_t *query_in,
                                rodsPath_t *rods_path,
                                const char *attr_name);

/**
 * Add a clause to a query to list AVUs on a collection, optionally
 * restricting results to a specific attribute.
 *
 * @param[out] query_in      The query to update.
 * @param[in]  rods_path     The target collection.
 * @param[in]  attr_name     An attribute name.
 *
 * @return The modified query.
 */
genQueryInp_t *prepare_col_list(genQueryInp_t *query_in,
                                rodsPath_t *rods_path,
                                const char *attr_name);

genQueryInp_t *prepare_obj_acl_list(genQueryInp_t *query_in,
                                    rodsPath_t *rods_path);

genQueryInp_t *prepare_col_acl_list(genQueryInp_t *query_in,
                                    rodsPath_t *rods_path);

genQueryInp_t *prepare_obj_repl_list(genQueryInp_t *query_in,
                                     rodsPath_t *rods_path);

//genQueryInp_t *prepare_obj_tps_list(genQueryInp_t *query_in,
//                                    rodsPath_t *rods_path);

genQueryInp_t *prepare_col_tps_list(genQueryInp_t *query_in,
                                    rodsPath_t *rods_path);

genQueryInp_t *prepare_obj_acl_search(genQueryInp_t *query_in,
                                      const char *user_id,
                                      const char *perm_id);

genQueryInp_t *prepare_col_acl_search(genQueryInp_t *query_in,
                                      const char *user_id,
                                      const char *access_level);

genQueryInp_t *prepare_obj_avu_search(genQueryInp_t *query_in,
                                      const char *attr_name,
                                      const char *attr_value,
                                      const char *operator);

genQueryInp_t *prepare_obj_avu_search_lim(genQueryInp_t *query_in,
                                          const char *attr_name,
                                          const char *attr_value,
                                          const char *operator);

genQueryInp_t *prepare_col_avu_search(genQueryInp_t *query_in,
                                      const char *attr_name,
                                      const char *attr_value,
                                      const char *operator);

genQueryInp_t *prepare_obj_cre_search(genQueryInp_t *query_in,
                                      const char *raw_timestamp,
                                      const char *operator);

genQueryInp_t *prepare_obj_mod_search(genQueryInp_t *query_in,
                                      const char *raw_timestamp,
                                      const char *operator);

genQueryInp_t *prepare_col_cre_search(genQueryInp_t *query_in,
                                      const char *raw_timestamp,
                                      const char *operator);

genQueryInp_t *prepare_col_mod_search(genQueryInp_t *query_in,
                                      const char *raw_timestamp,
                                      const char *operator);

genQueryInp_t *prepare_path_search(genQueryInp_t *query_in,
                                   const char *root_path);

genQueryInp_t *prepare_user_search(genQueryInp_t *query_in,
                                   const char *user_name);

specificQueryInp_t *prepare_specific_query(specificQueryInp_t *squery_in,
                                           const char *sql, json_t *args);

query_format_in_t *make_query_format_from_sql(const char *sql);

const char *irods_get_sql_for_specific_alias(rcComm_t *conn,
                                             const char *alias);

query_format_in_t *prepare_specific_labels(rcComm_t *conn, const char *sql);

void free_squery_input(specificQueryInp_t *squery_in);

void free_specific_labels(query_format_in_t *format);

genQueryInp_t *limit_to_newest_repl(genQueryInp_t *query_in);

#endif // _BATON_QUERY_H
