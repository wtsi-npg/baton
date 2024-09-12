/**
 * Copyright (C) 2013, 2014, 2015, 2017, 2019, 2021, 2024 Genome
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
 * @file json.h
 * @author Keith James <kdj@sanger.ac.uk>
 * @author Joshua C. Randall <jcrandall@alum.mit.edu>
 */

#ifndef _BATON_JSON_H
#define _BATON_JSON_H

#include <jansson.h>

#include "config.h"
#include "error.h"

// Error reporting
#define JSON_ERROR_KEY             "error"
#define JSON_ERROR_CODE_KEY        "code"
#define JSON_ERROR_MSG_KEY         "message"

// File/directory, data object/collection, properties
#define JSON_ZONE_KEY              "zone"

#define JSON_DIRECTORY_KEY         "directory"
#define JSON_DIRECTORY_SHORT_KEY   "dir"
#define JSON_FILE_KEY              "file"
#define JSON_COLLECTION_KEY        "collection"
#define JSON_COLLECTION_SHORT_KEY  "coll"
#define JSON_DATA_OBJECT_KEY       "data_object"
#define JSON_DATA_OBJECT_SHORT_KEY "obj"
#define JSON_DATA_KEY              "data"

#define JSON_CONTENTS_KEY          "contents"
#define JSON_SIZE_KEY              "size"
#define JSON_CHECKSUM_KEY          "checksum"
#define JSON_TIMESTAMPS_KEY        "timestamps"
#define JSON_TIMESTAMPS_SHORT_KEY  "time"

// Permissions
#define JSON_ACCESS_KEY            "access"
#define JSON_OWNER_KEY             "owner"
#define JSON_LEVEL_KEY             "level"

// Metadata attributes, values
#define JSON_AVUS_KEY              "avus"
#define JSON_ATTRIBUTE_KEY         "attribute"
#define JSON_ATTRIBUTE_SHORT_KEY   "a"
#define JSON_VALUE_KEY             "value"
#define JSON_VALUE_SHORT_KEY       "v"
#define JSON_UNITS_KEY             "units"
#define JSON_UNITS_SHORT_KEY       "u"

#define JSON_CREATED_KEY           "created"
#define JSON_CREATED_SHORT_KEY     "c"
#define JSON_MODIFIED_KEY          "modified"
#define JSON_MODIFIED_SHORT_KEY    "m"

// Resources and replicates
#define JSON_REPLICATE_KEY         "replicates"
#define JSON_REPLICATE_SHORT_KEY   "rep"
#define JSON_REPLICATE_NUMBER_KEY  "number"
#define JSON_REPLICATE_STATUS_KEY  "valid"
#define JSON_RESOURCE_KEY          "resource"
#define JSON_RESOURCE_TYPE_KEY     "type"
#define JSON_RESOURCE_HIER_KEY     "hierarchy"
#define JSON_LOCATION_KEY          "location"

// Metadata genquery API operations
#define JSON_OPERATOR_KEY          "operator"
#define JSON_OPERATOR_SHORT_KEY    "o"
#define JSON_ARGS_KEY              "args"
#define JSON_ARGS_SHORT_KEY        "?"
#define JSON_ARG_META_ADD          "add"
#define JSON_ARG_META_REM          "rem"

// SQL specific query operations
#define JSON_SPECIFIC_KEY          "specific"
#define JSON_SQL_KEY               "sql"
#define JSON_SQL_SHORT_KEY         "s"

// baton operations
#define JSON_TARGET_KEY            "target"
#define JSON_RESULT_KEY            "result"
#define JSON_SINGLE_RESULT_KEY     "single"
#define JSON_MULTIPLE_RESULT_KEY   "multiple"
#define JSON_OP_KEY                "operation"
#define JSON_OP_SHORT_KEY          "op"

#define JSON_CHMOD_OP              "chmod"
#define JSON_CHECKSUM_OP           "checksum"
#define JSON_GET_OP                "get"
#define JSON_LIST_OP               "list"
#define JSON_METAMOD_OP            "metamod"
#define JSON_METAQUERY_OP          "metaquery"
#define JSON_PUT_OP                "put"
#define JSON_MOVE_OP               "move"
#define JSON_RM_OP                 "remove"
#define JSON_MKCOLL_OP             "mkdir"
#define JSON_RMCOLL_OP             "rmdir"

#define JSON_OP_ARGS_KEY           "arguments"
#define JSON_OP_ARGS_SHORT_KEY     "args"

#define JSON_OP_ACL                "acl"
#define JSON_OP_AVU                "avu"
#define JSON_OP_PRINT_CHECKSUM     "checksum"
#define JSON_OP_CALCULATE_CHECKSUM "checksum"
#define JSON_OP_VERIFY_CHECKSUM    "verify"
#define JSON_OP_FORCE              "force"
#define JSON_OP_COLLECTION         "collection"
#define JSON_OP_CONTENTS           "contents"
#define JSON_OP_OBJECT             "object"
#define JSON_OP_OPERATION          "operation"
#define JSON_OP_RAW                "raw"
#define JSON_OP_RECURSE            "recurse"
#define JSON_OP_REPLICATE          "replicate"
#define JSON_OP_SAVE               "save"
#define JSON_OP_SINGLE_SERVER      "single-server"
#define JSON_OP_SIZE               "size"
#define JSON_OP_TIMESTAMP          "timestamp"
#define JSON_OP_PATH               "path"

#define VALID_REPLICATE   "1"
#define INVALID_REPLICATE "0"

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
 * Return a JSON object representing a specific query from a
 * JSON object.
 *
 * @param[in]                A JSON object.
 * @param[out] error         An error report struct.
 *
 * @return A JSON object on success, NULL on error.
 */
json_t *get_specific(json_t *object, baton_error_t *error);

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

const char* get_query_collection(json_t *object, baton_error_t *error);

const char *get_collection_value(json_t *object, baton_error_t *error);

const char *get_created_timestamp(json_t *object, baton_error_t *error);

const char *get_modified_timestamp(json_t *object, baton_error_t *error);

const char *get_checksum(json_t *object, baton_error_t *error);

const char* get_replicate_num(json_t *object, baton_error_t *error);

const char *get_avu_attribute(json_t *avu, baton_error_t *error);

const char *get_avu_value(json_t *avu, baton_error_t *error);

const char *get_avu_units(json_t *avu, baton_error_t *error);

const char *get_avu_operator(json_t *avu, baton_error_t *error);

const char *get_specific_sql(json_t *sql, baton_error_t *error);

/**
 * Return a JSON array representing arguments from a JSON object
 * representing an iRODS specific query. If there are no
 * arguments, an empty JSON array will be returned.
 *
 * @param[in]                A JSON object.
 * @param[out] error         An error report struct.
 *
 * @return A JSON array on success, NULL on error. If a JSON
 *         array is returned, the caller must call json_decref
 *         on it when done using it in order to free its memory.
 */
json_t *get_specific_args(json_t *sql, baton_error_t *error);

const char *get_access_owner(json_t *access, baton_error_t *error);

const char *get_access_level(json_t *access, baton_error_t *error);

const char *get_access_zone(json_t *access, baton_error_t *error);

const char *get_timestamp_operator(json_t *timestamp, baton_error_t *error);

const char *get_operation(json_t *object, baton_error_t *error);

json_t *get_operation_args(json_t *envelope, baton_error_t *error);

json_t *get_operation_target(json_t *envelope, baton_error_t *error);

const char *get_op_path(json_t *operation_args, baton_error_t *error);

int has_operation(json_t *object);

int has_operation_args(json_t *object);

int has_operation_target(json_t *envelope);

int has_op_path(json_t *operation_args);

int op_acl_p(json_t *operation_args);

int op_avu_p(json_t *operation_args);

int op_print_checksum_p(json_t *operation_args);

int op_calculate_checksum_p(json_t *operation_args);

int op_verify_checksum_p(json_t *operation_args);

int op_force_p(json_t *operation_args);

int op_collection_p(json_t *operation_args);

int op_contents_p(json_t *operation_args);

int op_object_p(json_t *operation_args);

int op_operation_p(json_t *operation_args);

int op_raw_p(json_t *operation_args);

int op_recurse_p(json_t *operation_args);

int op_replicate_p(json_t *operation_args);

int op_save_p(json_t *operation_args);

int op_single_server_p(json_t *operation_args);

int op_size_p(json_t *operation_args);

int op_timestamp_p(json_t *operation_args);

int has_checksum(json_t *object);

int has_collection(json_t *object);

int has_acl(json_t *object);

int has_timestamps(json_t *object);

int has_created_timestamp(json_t *object);

int has_modified_timestamp(json_t *object);

int contains_avu(json_t *avus, json_t *avu);

int represents_collection(json_t *object);

int represents_data_object(json_t *object);

int represents_directory(json_t *object);

int represents_file(json_t *object);

int add_collection(json_t *object, const char *coll_name, baton_error_t *error);

int add_metadata(json_t *object, json_t *avus, baton_error_t *error);

int add_permissions(json_t *object, json_t *perms, baton_error_t *error);

int add_contents(json_t *object, json_t *contents, baton_error_t *error);

int add_timestamps(json_t *object, const char *created, const char *modified,
                   const char *replicate, baton_error_t *error);

int add_replicates(json_t *object, json_t *replicates, baton_error_t *error);

int add_checksum(json_t *object, json_t *checksum, baton_error_t *error);

/**
 * Modify a JSON object by adding a property whose value is the result
 * of an operation. The property key will depend on the type of of the
 * value.
 *
 * @param[object]            A JSON object.
 * @param[result]            A JSON object.
 * @param[out] error         An error report struct.
 *
 * @return A JSON array on success, NULL on error./
 */
int add_result(json_t *object, json_t *result, baton_error_t *error);

int add_error_report(json_t *target, baton_error_t *error);

json_t *make_timestamp(const char* key, const char *value, const char *format,
                       const char *replicate, baton_error_t *error);

json_t *make_replicate(const char *resource, const char *location,
                       const char *checksum, const char *replicate,
                       const char *status, baton_error_t *error);

json_t *checksum_to_json(char *checksum, baton_error_t *error);

json_t *data_object_parts_to_json(const char *coll_name, const char *data_name,
                                  baton_error_t *error);

json_t *data_object_path_to_json(const char *path, baton_error_t *error);

json_t *collection_path_to_json(const char *path, baton_error_t *error);

char *json_to_checksum(json_t *object, baton_error_t *error);

char *json_to_path(json_t *object, baton_error_t *error);

char *json_to_collection_path(json_t *object, baton_error_t *error);

char *json_to_local_path(json_t *object, baton_error_t *error);

char *make_in_op_value(json_t *avu, baton_error_t *error);

void print_json_stream(json_t *json, FILE *stream);

void print_json(json_t *json);

#endif // _BATON_JSON_H
