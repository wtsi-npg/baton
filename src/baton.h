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
 * @file baton.h
 * @author Keith James <kdj@sanger.ac.uk>
 */

#ifndef _BATON_H
#define _BATON_H

#include <stdarg.h>
#include <jansson.h>
#include "rodsClient.h"
#include "rcConnect.h"
#include "rodsPath.h"

#define BATON_CAT "baton"

#define MAX_NUM_CONDITIONALS 20

#define META_ADD_NAME "add"
#define META_REM_NAME "rem"

/**
 *  @enum log_level
 *  @brief Log message levels.
 */
typedef enum {
    FATAL,
    ERROR,
    WARN,
    NOTICE,
    INFO,
    DEBUG
} log_level;

/**
 *  @enum metadata_op
 *  @brief AVU metadata operations.
 */
typedef enum {
    /** Add an AVU. */
    META_ADD,
    /** Remove an AVU. */
    META_REM,
    /** Query AVUs */
    META_QUERY
} metadata_op;

/**
 *  @struct metadata_op
 *  @brief AVU metadata operation inputs.
 */
typedef struct {
    /** The operation to perform. */
    metadata_op op;
    /** The type argument for the iRODS path i.e. -d or -C. */
    char *type_arg;
    /** The resolved iRODS path. */
    rodsPath_t *rods_path;
    /** The AVU attribute name. */
    char *attr_name;
    /** The AVU attribute value. */
    char *attr_value;
    /** The AVU attribute units. */
    char *attr_units;
} mod_metadata_in;

typedef struct {
    /** The ICAT column to match e.g. COL_META_DATA_ATTR_NAME */
    int column;
    /** The operator to use e.g. "=", "<", ">" */
    char* operator;
    /** The value to match */
    char* value;
} query_cond;

/**
 * Log an error through the underlying logging mechanism.  This
 * function exists to abstract the logging implementation.
 *
 * @param[in] level    The logging level.
 * @param[in] category The log message category.  Categories are based
 * on e.g. program subsystem.
 * @param[in] format    The logging format string or template.
 * @param[in] arguments The format arguments.
 */
void logmsg(log_level level, const char *category, const char *format, ...);

/**
 * Log the current iRODS error stack through the underlying logging
 * mechanism.
 *
 * @param[in] level    The logging level.
 * @param[in] category The log message category.
 * @param[in] error    The iRODS error state.
 */
void log_rods_errstack(log_level level, const char *category, rError_t *error);

/**
 * Log the current JSON error state through the underlying logging
 * mechanism.
 *
 * @param[in] level    The logging level.
 * @param[in] category The log message category.
 * @param[in] error    The JSON error state.
 */
void log_json_error(log_level level, const char *category,
                    json_error_t *error);

/**
 * Test that a connection can be made to the server.
 *
 * @return 1 on success, 0 on failure.
 */
int is_irods_available();

/**
 * Log into iRODS using an pre-defined environment.
 *
 * @param[in] env A populated iRODS environment.
 *
 * @return An open connection to the iRODS server or NULL on error.
 */
rcComm_t *rods_login(rodsEnv *env);

/**
 * Initialise an iRODS path by copying a string into its inPath.
 *
 * @param[out] rodspath  An iRODS path.
 * @param[in]  inpath    A string representing an unresolved iRODS path.
 *
 * @return 0 on success, -1 on failure.
 */
int init_rods_path(rodsPath_t *rodspath, char *inpath);

/**
 * Initialise and resolve an iRODS path by copying a string into its
 * inPath, parsing it and resolving it on the server.
 *
 * @param[in]  conn      An open iRODS connection.
 * @param[in]  env       A populated iRODS environment.
 * @param[out] rodspath  An iRODS path.
 * @param[in]  inpath    A string representing an unresolved iRODS path.
 *
 * @return 0 on success, iRODS error code on failure.
 */
int resolve_rods_path(rcComm_t *conn, rodsEnv *env,
                      rodsPath_t *rods_path, char *inpath);

json_t *rods_path_to_json(rcComm_t *conn, rodsPath_t *rods_path);

/**
 * List metadata of a specified data object or collection.
 *
 * @param[in]  conn       An open iRODS connection.
 * @param[out] rodspath   An iRODS path.
 * @param[in] attr_name   An attribute name to limit the values returned.
 *                        Optional, NULL means return all metadata.
 *
 * @return A newly constructed JSON array of AVU JSON objects.
 */
json_t *list_metadata(rcComm_t *conn, rodsPath_t *rods_path, char *attr_name);

json_t *search_metadata(rcComm_t *conn, char *attr_name, char *attr_value);

/**
 * Apply a metadata operation to an AVU on a resolved iRODS path.
 *
 * @param[in] conn        An open iRODS connection.
 * @param[in] rodspath    A resolved iRODS path.
 * @param[in] op          An operation to apply e.g. ADD, REMOVE.
 * @param[in] attr_name   The attribute name.
 * @param[in] attr_value  The attribute value.
 * @param[in] attr_unit   The attribute unit (the empty string for none).
 *
 * @return 0 on success, iRODS error code on failure.
 */
int modify_metadata(rcComm_t *conn, rodsPath_t *rodspath, metadata_op op,
                    char *attr_name, char *attr_value, char *attr_unit);

/**
 * Allocate a new iRODS generic query (see rodsGenQuery.h).
 *
 * @param[in] max_rows    Maximum number of rows to return.
 * @param[in] num_columns The number of columns to select.
 * @param[in] columns     The columns to select.
 *
 * @return A pointer to a new genQueryInp_t which must be freed using
 * @ref free_query_input
 */
genQueryInp_t *make_query_input(int max_rows, int num_columns,
                                const int columns[]);
/**
 * Free memory used by an iRODS generic query (see rodsGenQuery.h).
 *
 * @param[in] query_input The query to free.
 *
 * @ref make_query_input
 */
void free_query_input(genQueryInp_t *query_input);

genQueryInp_t *add_query_conds(genQueryInp_t *query_input, int num_conds,
                               const query_cond conds[]);

int query_and_print(rcComm_t *conn, genQueryInp_t *query_input,
                    const char *labels[]);

/**
 * Execute a general query and obtain results as a JSON array of objects.
 * Columns in the query are mapped to JSON object properties specified
 * by the labels argument.
 *
 * @param[in]  conn         An open iRODS connection.
 * @param[in]  query_input  A populated query input.
 * @param[out] query_output A query output struct to receive results.
 * @param[in] labels        An array of as many labels as there were columns
 *                          selected in the query.
 *
 * @return A newly constructed JSON array of objects, one per result row. The
 * caller must free this after use.
 */
json_t *do_query(rcComm_t *conn, genQueryInp_t *query_input,
                 genQueryOut_t *query_output, const char *labels[]);

/**
 * Construct a JSON array of objects from a query output. Columns in the
 * query are mapped to JSON object properties specified by the labels
 * argument.
 *
 * @param[in] query_output  A populated query output.
 * @param[in] labels        An array of as many labels as there were columns
 *                          selected in the query.
 *
 * @return A newly constructed JSON array of objects, one per result row. The
 * caller must free this after use.
 */
json_t *make_json_objects(genQueryOut_t *query_output, const char *labels[]);

void print_json(json_t* results);

#endif // BATON_H
