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

#include <stdarg.h>

#include <jansson.h>

#include "rcConnect.h"
#include "rodsPath.h"

#define BATON_CAT "baton"
#define BMETA_CAT "baton_bmeta"

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
} metadata_op;

static char *metadata_ops[] = {
     "add",
     "rem",
};

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
void logmsg(log_level level, const char* category, const char *format, ...);

void log_rods_errstack(log_level level, const char* category, rError_t *Err);

void log_json_error(log_level level, const char* category,
                    json_error_t *error);

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
