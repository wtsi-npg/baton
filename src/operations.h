/**
 * Copyright (C) 2013, 2014, 2015, 2016, 2017, 2018, 2019 Genome
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
 * @file operations.h
 * @author Keith James <kdj@sanger.ac.uk>
 */

#ifndef _BATON_OPERATIONS_H
#define _BATON_OPERATIONS_H

#include <rodsClient.h>

#include <jansson.h>

#include "config.h"

/**
 *  @enum metadata_op
 *  @brief AVU metadata operations.
 */
typedef enum {
    /** Add an AVU. */
    META_ADD,
    /** Remove an AVU. */
    META_REM
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
    SEARCH_OBJECTS     = 1 << 8,
    /** Unsafely resolve relative paths */
    UNSAFE_RESOLVE     = 1 << 9,
    /** Print replicate details for data objects */
    PRINT_REPLICATE    = 1 << 10,
    /** Print checksums for data objects */
    PRINT_CHECKSUM     = 1 << 11,
    /** Calculate checksums for data objects */
    CALCULATE_CHECKSUM = 1 << 12,
    /** Add an AVU */
    ADD_AVU            = 1 << 13,
    /** Remove an AVU */
    REMOVE_AVU         = 1 << 14,
    /** Recursive operation on collections */
    RECURSIVE          = 1 << 15,
    /** Save files */
    SAVE_FILES         = 1 << 16,
    /** Flush output */
    FLUSH              = 1 << 17,
    /** Force an operation */
    FORCE              = 1 << 18,
    /** Avoid any operations that contact servers other than rodshost */
    SINGLE_SERVER      = 1 << 19,
    /** Use advisory write lock on server */
    WRITE_LOCK         = 1 << 20
} option_flags;

typedef struct operation_args {
    option_flags flags;
    size_t buffer_size;
    char *zone_name;
    char *path;
    unsigned long max_connect_time;
} operation_args_t;

/**
 * Typedef for baton JSON document processing functions.
 *
 * @param[in]      env          A populated iRODS environment.
 * @param[in]      conn         An open iRODS connection.
 * @param[in,out]  target       A baton JSON document.
 * @param[in]      flags        Function behaviour options.
 * @param[out]     error        An error report struct.
 * @param[in]      args         Optional arguments, char * zone name,
 *                              size_t transfer buffer size.
 *
 * @return json_t on success, which may be NULL if the operation
 * is void e.g. side-effect only operations.
 */
typedef json_t *(*baton_json_op) (rodsEnv *env,
                                  rcComm_t *conn,
                                  json_t *target,
                                  operation_args_t *args,
                                  baton_error_t *error);

/**
 * Process a stream of baton JSON documents by executing the specifed
 * function on each one.
 *
 * @param[in]  input        A file handle.
 * @param[fn]  fn           A function.
 * @param[in]  flags        Function behaviour options.
 * @param[in]  zone_name    Zone name, char * (optional).
 * @param[in]  buffer_size  Data transfer buffer, size_t (optional).
 *
 * @return 0 on success, iRODS error code on failure.
 */
int do_operation(FILE *input, baton_json_op fn, operation_args_t *args);

json_t *baton_json_dispatch_op(rodsEnv *env, rcComm_t *conn,
                               json_t *target, operation_args_t *args,
                               baton_error_t *error);

json_t *baton_json_list_op(rodsEnv *env, rcComm_t *conn,
                           json_t *target, operation_args_t *args,
                           baton_error_t *error);

json_t *baton_json_chmod_op(rodsEnv *env, rcComm_t *conn,
                            json_t *target, operation_args_t *args,
                            baton_error_t *error);

json_t *baton_json_checksum_op(rodsEnv *env, rcComm_t *conn,
                               json_t *target, operation_args_t *args,
                               baton_error_t *error);

json_t *baton_json_metaquery_op(rodsEnv *env, rcComm_t *conn,
                                json_t *target, operation_args_t *args,
                                baton_error_t *error);

json_t *baton_json_metamod_op(rodsEnv *env, rcComm_t *conn,
                              json_t *target, operation_args_t *args,
                              baton_error_t *error);

json_t *baton_json_get_op(rodsEnv *env, rcComm_t *conn,
                          json_t *target, operation_args_t *args,
                          baton_error_t *error);

json_t *baton_json_put_op(rodsEnv *env, rcComm_t *conn,
                          json_t *target, operation_args_t *args,
                          baton_error_t *error);

json_t *baton_json_write_op(rodsEnv *env, rcComm_t *conn,
                            json_t *target, operation_args_t *args,
                            baton_error_t *error);

json_t *baton_json_move_op(rodsEnv *env, rcComm_t *conn,
                           json_t *target, operation_args_t *args,
                           baton_error_t *error);

json_t *baton_json_rm_op(rodsEnv *env, rcComm_t *conn,
                         json_t *target, operation_args_t *args,
                         baton_error_t *error);

json_t *baton_json_mkcoll_op(rodsEnv *env, rcComm_t *conn,
                             json_t *target, operation_args_t *args,
                             baton_error_t *error);

json_t *baton_json_rmcoll_op(rodsEnv *env, rcComm_t *conn,
                             json_t *target, operation_args_t *args,
                             baton_error_t *error);

int check_str_arg(const char *arg_name, const char *arg_value,
                  size_t arg_size, baton_error_t *error);

#endif // _BATON_OPERATIONS_H
