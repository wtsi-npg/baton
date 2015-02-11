/**
 * Copyright (c) 2013, 2014, 2015 Genome Research Ltd. All rights
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
 * @file baton.h
 * @author Keith James <kdj@sanger.ac.uk>
 */

#ifndef _BATON_H
#define _BATON_H

#include <jansson.h>

#include "config.h"

#ifdef HAVE_IRODS3
#include "rcConnect.h"
#include "rodsClient.h"
#include "rodsPath.h"
#endif

#ifdef HAVE_IRODS4
#include "rcConnect.hpp"
#include "rodsClient.hpp"
#include "rodsPath.hpp"
#endif

#include "error.h"
#include "query.h"
#include "utilities.h"

#define MAX_CLIENT_NAME_LEN   512

#define META_ADD_NAME "add"
#define META_REM_NAME "rm"

#define FILE_SIZE_UNITS "KB"

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

typedef enum {
    /** Non-recursive operation */
    NO_RECURSE,
    /** Recursive operation */
    RECURSE
} recursive_op;

typedef enum {
    /** Print AVUs on collections and data objects */
    PRINT_AVU          = 1 << 0,
    /** Print ACLs on collections and data objects */
    PRINT_ACL          = 1 << 1,
    /** Print the contents of collections */
    PRINT_CONTENTS     = 1 << 2,
    /** Print timestamps on collections and data objects */
    PRINT_TIMESTAMP    = 1 << 3,
    /** Print file sizes for data objects */
    PRINT_SIZE         = 1 << 4,
    /** Pretty-print JSON */
    PRINT_PRETTY       = 1 << 5,
    /** Print raw output */
    PRINT_RAW          = 1 << 6,
    /** Search collection AVUs */
    SEARCH_COLLECTIONS = 1 << 7,
    /** Search data object AVUs */
    SEARCH_OBJECTS     = 1 << 8

} option_flags;

/**
 *  @struct metadata_op
 *  @brief AVU metadata operation inputs.
 */
typedef struct mod_metadata_in {
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
} mod_metadata_in_t;


/**
 * Test that a connection can be made to the server.
 *
 * @return 1 on success, 0 on failure.
 */
int is_irods_available();

/**
 * Set the SP_OPTION environment variable so that the 'ips' command
 * knows the name of a client program.
 *
 * @param[in] name The client name.
 *
 * @return The return value of 'setenv'.
 */
int declare_client_name(const char *name);

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
 * @param[out] rods_path An iRODS path.
 * @param[in]  inpath    A string representing an unresolved iRODS path.
 *
 * @return 0 on success, -1 on failure.
 */
int init_rods_path(rodsPath_t *rods_path, char *inpath);

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
 * Initialise and set an iRODS path by copying a string into both its
 * inPath and outPath and then resolving it on the server. The path is not
 * parsed, so must be derived from an existing parsed path.
 *
 * @param[in]  conn      An open iRODS connection.
 * @param[in]  env       A populated iRODS environment.
 * @param[out] rodspath  An iRODS path.
 * @param[in]  path      A string representing an unresolved iRODS path.
 *
 * @return 0 on success, iRODS error code on failure.
 */
int set_rods_path(rcComm_t *conn, rodsPath_t *rods_path, char *path);

json_t *get_user(rcComm_t *conn, const char *user_name, baton_error_t *error);

/**
 * Return a JSON representation of the content of a resolved iRODS
 * path (data object or collection). In the case of a data object,
 * return the representation of that path. In the case of an
 * collection, return a JSON array containing zero or more JSON
 * representations of its contents.
 *
 * @param[in]  conn         An open iRODS connection.
 * @param[in]  rodspath     An iRODS path.
 * @param[in]  option_flags Result print options.
 * @param[out] error        An error report struct.
 *
 * @return A new struct representing the path content, which must be
 * freed by the caller.
 */
json_t *list_path(rcComm_t *conn, rodsPath_t *rods_path, option_flags flags,
                  baton_error_t *error);


json_t *ingest_path(rcComm_t *conn, rodsPath_t *rods_path, option_flags flags,
                    size_t buffer_size, baton_error_t *error);

int write_path_to_file(rcComm_t *conn, rodsPath_t *rods_path,
                       const char *local_path, size_t buffer_size,
                       baton_error_t *error);

int write_path_to_stream(rcComm_t *conn, rodsPath_t *rods_path, FILE *out,
                         size_t buffer_size, baton_error_t *error);

/**
 * Return a JSON representation of the created and modified timestamps
 * of a resolved iRODS path (data object or collection).
 *
 * @param[in]  conn      An open iRODS connection.
 * @param[in]  rodspath  An iRODS path.
 * @param[out] error     An error report struct.
 *
 * @return A new struct representing the timestamps, which must be
 * freed by the caller.
 */
json_t *list_timestamps(rcComm_t *conn, rodsPath_t *rods_path,
                        baton_error_t *error);

/**
 * Return a JSON representation of the access control list of a
 * resolved iRODS path (data object or collection).
 *
 * @param[in]  conn      An open iRODS connection.
 * @param[in]  rodspath  An iRODS path.
 * @param[out] error     An error report struct.
 *
 * @return A new struct representing the path access control list,
 * which must be freed by the caller.
 */
json_t *list_permissions(rcComm_t *conn, rodsPath_t *rods_path,
                         baton_error_t *error);

/**
 * List metadata of a specified data object or collection.
 *
 * @param[in]  conn       An open iRODS connection.
 * @param[out] rodspath   An iRODS path.
 * @param[in]  attr_name  An attribute name to limit the values returned.
 *                        Optional, NULL means return all metadata.
 * @param[out] error      An error report struct.
 *
 * @return A newly constructed JSON array of AVU JSON objects.
 */
json_t *list_metadata(rcComm_t *conn, rodsPath_t *rods_path, char *attr_name,
                      baton_error_t *error);

/**
 * Search metadata to find matching data objects and collections.
 *
 * @param[in]  conn         An open iRODS connection.
 * @param[in]  query        A JSON query specification which includes the
 *                          attribute name and value to match. It may also have
 *                          an iRODS path to limit search scope. Only results
 *                          under this path will be returned. Omitting the path
 *                          means the search will be global.
 * @param[in]  zone_name    An iRODS zone name. Optional, NULL means the current
 *                          zone.
 * @param[in]  option_flags Result print options.
 * @param[out] error        An error report struct.
 *
 * @return A newly constructed JSON array of JSON result objects.
 */
json_t *search_metadata(rcComm_t *conn, json_t *query, char *zone_name,
                        option_flags flags, baton_error_t *error);

/**
 * Modify the access control list of a resolved iRODS path.
 *
 * @param[in]     conn      An open iRODS connection.
 * @param[out]    rodspath  An iRODS path.
 * @param[in]     recurse   Recurse into collections, one of RECURSE,
                            NO_RECURSE.
 * @param[in]     perms     An iRODS access control mode (read, write etc.)
 * @param[in,out] error     An error report struct.
 *
 * @return 0 on success, iRODS error code on failure.
 */
int modify_permissions(rcComm_t *conn, rodsPath_t *rods_path,
                       recursive_op recurse, char *owner_specifier,
                       char *access_level,  baton_error_t *error);

/**
 * Modify the access control list of a resolved iRODS path.  The
 * functionality is identical to modify_permission, except that the
 * access control argument is a JSON struct.
 *
 * @param[in]  conn       An open iRODS connection.
 * @param[out] rodspath   An iRODS path.
 * @param[in]  recurse    Recurse into collections, one of RECURSE, NO_RECURSE.
 * @param[in]  perms      A JSON access control object.
 * @param[out] error      An error report struct.
 *
 * @return 0 on success, iRODS error code on failure.
 */
int modify_json_permissions(rcComm_t *conn, rodsPath_t *rods_path,
                            recursive_op recurse, json_t *perms,
                            baton_error_t *error);

/**
 * Apply a metadata operation to an AVU on a resolved iRODS path.
 *
 * @param[in]     conn        An open iRODS connection.
 * @param[in]     rodspath    A resolved iRODS path.
 * @param[in]     op          An operation to apply e.g. ADD, REMOVE.
 * @param[in]     attr_name   The attribute name.
 * @param[in]     attr_value  The attribute value.
 * @param[in]     attr_unit   The attribute unit (the empty string for none).
 * @param[in,out] error       An error report struct.
 *
 * @return 0 on success, iRODS error code on failure.
 */
int modify_metadata(rcComm_t *conn, rodsPath_t *rodspath, metadata_op op,
                    char *attr_name, char *attr_value, char *attr_unit,
                    baton_error_t *error);

/**
 * Apply a metadata operation to an AVU on a resolved iRODS path. The
 * functionality is identical to modify_metadata, except that the AVU is
 * a JSON struct argument, with optional units.
 *
 * @param[in]  conn        An open iRODS connection.
 * @param[in]  rodspath    A resolved iRODS path.
 * @param[in]  op          An operation to apply e.g. ADD, REMOVE.
 * @param[in]  avu         The JSON AVU.
 * @param[out] error       An error report struct.
 *
 * @return 0 on success, iRODS error code on failure.
 * @ref modify_metadata
 */
int modify_json_metadata(rcComm_t *conn, rodsPath_t *rods_path,
                         metadata_op operation, json_t *avu,
                         baton_error_t *error);

#endif // _BATON_H
