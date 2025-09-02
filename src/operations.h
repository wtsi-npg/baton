/**
 * Copyright (C) 2013, 2014, 2015, 2016, 2017, 2018, 2019, 2021, 2022,
 * 2025 Genome Research Ltd. All rights reserved.
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

#include <pthread.h>
#include <rodsClient.h>

#include <jansson.h>

#include "baton.h"

typedef struct operation_args {
    option_flags flags;
    size_t buffer_size;
    char *zone_name;
    char *path;
    long max_connect_time;
} operation_args_t;

/**
 * Typedef for baton JSON document processing functions.
 *
 * @param[in]      session      A baton session providing environment and
 *                              connection context required by the operation.
 * @param[in,out]  target       A baton JSON document to read and/or modify.
 * @param[in]      args         Optional operation arguments (may be NULL),
 *                              including flags, buffer_size, zone_name, path,
 *                              and max_connect_time.
 * @param[out]     error        An error report struct to be populated on failure.
 *
 * @return json_t* on success, which may be NULL for side-effect-only operations.
 * Returns NULL on failure with details in 'error'.
 */
typedef json_t*(*baton_json_op)(baton_session_t *session,
                                json_t *target,
                                const operation_args_t *args,
                                baton_error_t *error);

/**
 * Process a stream of baton JSON documents by executing the specifed
 * function on each one.
 *
 * @param[in]  input        A file handle.
 * @param[fn]  fn           An operation function.
 * @param[in]  args         Operations arguments.
 *
 * @return 0 on success, error code on failure. The error code is suitable
 * for use as an exit code for the program calling do_operation.
 */
int do_operation(FILE *input, baton_json_op fn, operation_args_t *args);

json_t* baton_json_dispatch_op(baton_session_t *session,
                               json_t *target,
                               const operation_args_t *args,
                               baton_error_t *error);

json_t* baton_json_list_op(baton_session_t *session,
                           json_t *target,
                           const operation_args_t *args,
                           baton_error_t *error);

json_t* baton_json_chmod_op(baton_session_t *session,
                            json_t *target,
                            const operation_args_t *args,
                            baton_error_t *error);

json_t* baton_json_checksum_op(baton_session_t *session,
                               json_t *target,
                               const operation_args_t *args,
                               baton_error_t *error);

json_t* baton_json_metaquery_op(baton_session_t *session,
                                json_t *target,
                                const operation_args_t *args,
                                baton_error_t *error);

json_t* baton_json_metamod_op(baton_session_t *session,
                              json_t *target,
                              const operation_args_t *args,
                              baton_error_t *error);

json_t* baton_json_get_op(baton_session_t *session,
                          json_t *target,
                          const operation_args_t *args,
                          baton_error_t *error);

json_t* baton_json_put_op(baton_session_t *session,
                          json_t *target,
                          const operation_args_t *args,
                          baton_error_t *error);

json_t* baton_json_write_op(baton_session_t *session,
                            json_t *target,
                            const operation_args_t *args,
                            baton_error_t *error);

json_t* baton_json_move_op(baton_session_t *session,
                           json_t *target,
                           const operation_args_t *args,
                           baton_error_t *error);

json_t* baton_json_rm_op(baton_session_t *session,
                         json_t *target,
                         const operation_args_t *args,
                         baton_error_t *error);

json_t* baton_json_mkcoll_op(baton_session_t *session,
                             json_t *target,
                             const operation_args_t *args,
                             baton_error_t *error);

json_t* baton_json_rmcoll_op(baton_session_t *session,
                             json_t *target,
                             const operation_args_t *args,
                             baton_error_t *error);


#endif // _BATON_OPERATIONS_H