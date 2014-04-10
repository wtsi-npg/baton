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
 * @file json.h
 * @author Keith James <kdj@sanger.ac.uk>
 */

#ifndef _BATON_JSON_H
#define _BATON_JSON_H

#include <jansson.h>
#include "rodsClient.h"

#include "error.h"

#define JSON_ERROR_KEY             "error"
#define JSON_ERROR_CODE_KEY        "code"
#define JSON_ERROR_MSG_KEY         "message"

#define JSON_DATA_OBJECT_KEY       "data_object"
#define JSON_DATA_OBJECT_SHORT_KEY "obj"
#define JSON_COLLECTION_KEY        "collection"
#define JSON_COLLECTION_SHORT_KEY  "coll"
#define JSON_TIMESTAMPS_KEY        "timestamps"
#define JSON_TIMESTAMPS_SHORT_KEY  "time"

#define JSON_SIZE_KEY              "size"
#define JSON_AVUS_KEY              "avus"

#define JSON_ATTRIBUTE_KEY         "attribute"
#define JSON_ATTRIBUTE_SHORT_KEY   "a"
#define JSON_VALUE_KEY             "value"
#define JSON_VALUE_SHORT_KEY       "v"
#define JSON_UNITS_KEY             "units"
#define JSON_UNITS_SHORT_KEY       "u"
#define JSON_OPERATOR_KEY          "operator"
#define JSON_OPERATOR_SHORT_KEY    "o"

#define JSON_CREATED_KEY           "created"
#define JSON_CREATED_SHORT_KEY     "c"
#define JSON_MODIFIED_KEY          "modified"
#define JSON_MODIFIED_SHORT_KEY    "m"

#define JSON_ACCESS_KEY            "access"
#define JSON_OWNER_KEY             "owner"
#define JSON_LEVEL_KEY             "level"

#define JSON_USER_NAME_KEY         "user_name"
#define JSON_USER_ID_KEY           "user_id"
#define JSON_USER_TYPE_KEY         "user_type"
#define JSON_USER_ZONE_KEY         "user_zone"

/**
 * Add a new property containing error information to a JSON object.
 *
 * @param[in] object         A JSON object to modify.
 * @param[in] error          An error report struct.
 *
 * @return 0 on success, -1 on error.
 */
int add_error_value(json_t *object, baton_error_t *error);

/**
 * Convert error information to a JSON object.
 *
 * @param[in] error          An error report struct.
 *
 * @return A JSON object on success, NULL on error.
 */
json_t *error_to_json(baton_error_t *error);

/**
 * Return a JSON array representing an ACL from a JSON object
 * representing an iRODS path.
 *
 * @param[in]                A JSON object.
 * @param[out] error         An error report struct.
 *
 * @return A JSON array on success, NULL on error.
 */
json_t *get_acl(json_t *object, baton_error_t *error);

/**
 * Return a JSON array representing an AVUs from a JSON object
 * representing an iRODS path.
 *
 * @param[in]                A JSON object.
 * @param[out] error         An error report struct.
 *
 * @return A JSON array on success, NULL on error.
 */
json_t *get_avus(json_t *object, baton_error_t *error);

/**
 * Return a JSON array representing timestamps from a JSON object
 * representing an iRODS path.
 *
 * @param[in]                A JSON object.
 * @param[out] error         An error report struct.
 *
 * @return A JSON array on success, NULL on error.
 */
json_t *get_timestamps(json_t *object, baton_error_t *error);

const char *get_created_timestamp(json_t *object, baton_error_t *error);

const char *get_modified_timestamp(json_t *object, baton_error_t *error);

const char *get_avu_attribute(json_t *avu, baton_error_t *error);

const char *get_avu_value(json_t *avu, baton_error_t *error);

const char *get_avu_units(json_t *avu, baton_error_t *error);

const char *get_avu_operator(json_t *avu, baton_error_t *error);

const char *get_access_owner(json_t *access, baton_error_t *error);

const char *get_access_level(json_t *access, baton_error_t *error);

const char *get_timestamp_operator(json_t *timestamp, baton_error_t *error);

int has_acl(json_t *object);

int has_timestamps(json_t *object);

int has_created_timestamp(json_t *object);

int has_modified_timestamp(json_t *object);

int contains_avu(json_t *avus, json_t *avu);

int represents_collection(json_t *object);

int represents_data_object(json_t *object);

int add_metadata(json_t *object, json_t *avus, baton_error_t *error);

int add_permissions(json_t *object, json_t *perms, baton_error_t *error);

int add_timestamps(json_t *object, const char *created, const char *modified,
                   baton_error_t *error);

json_t *make_timestamp(const char* key, const char *value, const char *format,
                       baton_error_t *error);

json_t *data_object_parts_to_json(const char *coll_name, const char *data_name);

json_t *data_object_path_to_json(const char *path);

json_t *collection_path_to_json(const char *path);

json_t *query_args_to_json(const char *attr_name, const char *attr_value,
                           const char *root_path);

char *json_to_path(json_t *object, baton_error_t *error);

void print_json(json_t *json);

#endif // _BATON_JSON_H
