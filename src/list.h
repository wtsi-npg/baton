/**
 * Copyright (C) 2013, 2014, 2015, 2016, 2017, 2025 Genome Research Ltd. All
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
 * @file list.h
 * @author Keith James <kdj@sanger.ac.uk>
 */

#ifndef _BATON_LIST_H
#define _BATON_LIST_H

#include <jansson.h>

#include <rodsClient.h>

#include "config.h"
#include "error.h"
#include "json.h"
#include "json_query.h"
#include "operations.h"
#include "query.h"

json_t *list_checksum(rcComm_t *conn, rodsPath_t *rods_path,
                      baton_error_t *error);

/**
 * Return a JSON representation of the content of a resolved iRODS
 * path (data object or collection). In the case of a data object,
 * return the representation of that path. In the case of an
 * collection, return a JSON array containing zero or more JSON
 * representations of its contents.
 *
 * @param[in]  conn         An open iRODS connection.
 * @param[in]  rods_path    An iRODS path.
 * @param[in]  flags        Result print options.
 * @param[out] error        An error report struct.
 *
 * @return A new struct representing the path content, which must be
 * freed by the caller.
 */
json_t *list_path(rcComm_t *conn, rodsPath_t *rods_path, option_flags flags,
                  baton_error_t *error);

/**
 * Return a JSON representation of the access control list of a
 * resolved iRODS path (data object or collection).
 *
 * @param[in]  conn      An open iRODS connection.
 * @param[in]  rods_path An iRODS path.
 * @param[out] error     An error report struct.
 *
 * @return A new struct representing the path access control list,
 * which must be freed by the caller.
 */
json_t *list_permissions(rcComm_t *conn, rodsPath_t *rods_path,
                         baton_error_t *error);

/**
 * Return a JSON representation of the replicates of a resolved iRODS
 * path (data object).
 *
 * @param[in]  conn      An open iRODS connection.
 * @param[in]  rods_path An iRODS path.
 * @param[out] error     An error report struct.
 *
 * @return A new struct representing the path access control list,
 * which must be freed by the caller.
 */
json_t *list_replicates(rcComm_t *conn, rodsPath_t *rods_path,
                        baton_error_t *error);

/**
 * Return a JSON representation of the created and modified timestamps
 * of a resolved iRODS path (data object or collection).
 *
 * @param[in]  conn      An open iRODS connection.
 * @param[in]  rods_path An iRODS path.
 * @param[out] error     An error report struct.
 *
 * @return A new struct representing the timestamps, which must be
 * freed by the caller.
 */
json_t *list_timestamps(rcComm_t *conn, rodsPath_t *rods_path,
                        baton_error_t *error);

/**
 * List metadata of a specified data object or collection.
 *
 * @param[in]  conn       An open iRODS connection.
 * @param[out] rods_path  An iRODS path.
 * @param[in]  attr_name  An attribute name to limit the values returned.
 *                        Optional, NULL means return all metadata.
 * @param[out] error      An error report struct.
 *
 * @return A newly constructed JSON array of AVU JSON objects.
 */
json_t *list_metadata(rcComm_t *conn, rodsPath_t *rods_path, const char *attr_name,
                      baton_error_t *error);

#endif // _BATON_LIST_H
