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
 * @file json.c
 * @author Keith James <kdj@sanger.ac.uk>
 */

#include <errno.h>
#include <libgen.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <jansson.h>

#include "config.h"
#include "json.h"
#include "log.h"
#include "utilities.h"

static json_t *get_json_value(json_t *object, const char *name,
                              const char *key, const char *short_key,
                              baton_error_t *error);

static const char *get_string_value(json_t *object, const char *name,
                                    const char *key,
                                    const char *short_key,
                                    baton_error_t *error);

static const char *get_opt_string_value(json_t *object, const char *name,
                                        const char *key,
                                        const char *short_key,
                                        baton_error_t *error);

static int has_json_str_value(json_t *object, const char *key,
                              const char *short_key);

static char *make_dir_path(const char *path, baton_error_t *error);
static char *make_file_path(const char *path, const char *filename,
                            baton_error_t *error);

json_t *error_to_json(baton_error_t *error) {
    json_t *err = json_pack("{s:s, s:i}",
                            JSON_ERROR_MSG_KEY , error->message,
                            JSON_ERROR_CODE_KEY, error->code);
    if (!err) {
        logmsg(ERROR, "Failed to pack error '%s' as JSON", error->message);
    }

    return err;
}

int add_error_value(json_t *object, baton_error_t *error) {
    json_t *err = error_to_json(error);
    return json_object_set_new(object, JSON_ERROR_KEY, err);
}

json_t *get_acl(json_t *object, baton_error_t *error) {
    json_t *acl = get_json_value(object, "path spec", JSON_ACCESS_KEY, NULL,
                                 error);
    if (error->code != 0) goto error;
    if (!json_is_array(acl)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid '%s' attribute: not a JSON array",
                        JSON_ACCESS_KEY);
        goto error;
    }

    return acl;

error:
    return NULL;
}

json_t *get_avus(json_t *object, baton_error_t *error) {
    json_t *avus = get_json_value(object, "path spec", JSON_AVUS_KEY, NULL,
                                  error);
    if (error->code != 0) goto error;
    if (!json_is_array(avus)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid '%s' attribute: not a JSON array",
                        JSON_AVUS_KEY);
        goto error;
    }

    return avus;

error:
    return NULL;
}

json_t *get_timestamps(json_t *object, baton_error_t *error) {
    json_t *timestamps = get_json_value(object, "path spec",
                                        JSON_TIMESTAMPS_KEY,
                                        JSON_TIMESTAMPS_SHORT_KEY, error);
    if (error->code != 0) goto error;
    if (!json_is_array(timestamps)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid '%s' attribute: not a JSON array",
                        JSON_TIMESTAMPS_KEY);
        goto error;
    }

    return timestamps;

error:
    return NULL;
}

const char *get_collection_value(json_t *object, baton_error_t *error) {
    return get_string_value(object, "path spec", JSON_COLLECTION_KEY,
                            JSON_COLLECTION_SHORT_KEY, error);
}

const char *get_data_object_value(json_t *object, baton_error_t *error) {
    return get_opt_string_value(object, "path spec", JSON_DATA_OBJECT_KEY,
                                JSON_DATA_OBJECT_SHORT_KEY, error);
}

const char *get_directory_value(json_t *object, baton_error_t *error) {
    return get_opt_string_value(object, "path spec", JSON_DIRECTORY_KEY,
                                JSON_DIRECTORY_SHORT_KEY, error);
}

const char *get_file_value(json_t *object, baton_error_t *error) {
    return get_opt_string_value(object, "path spec", JSON_FILE_KEY,
                                NULL, error);
}

const char *get_query_collection(json_t *object, baton_error_t *error) {
    return get_opt_string_value(object, "path spec", JSON_COLLECTION_KEY,
                                JSON_COLLECTION_SHORT_KEY, error);
}

const char *get_created_timestamp(json_t *object, baton_error_t *error) {
    return get_string_value(object, "timestamps", JSON_CREATED_KEY,
                            JSON_CREATED_SHORT_KEY, error);
}

const char *get_modified_timestamp(json_t *object, baton_error_t *error) {
    return get_string_value(object, "timestamps", JSON_MODIFIED_KEY,
                            JSON_MODIFIED_SHORT_KEY, error);
}

const char *get_replicate_num(json_t *object, baton_error_t *error) {
    return get_opt_string_value(object, "timestamps", JSON_REPLICATE_KEY,
                                JSON_REPLICATE_SHORT_KEY, error);
}

const char *get_avu_attribute(json_t *avu, baton_error_t *error) {
    return get_string_value(avu, "AVU", JSON_ATTRIBUTE_KEY,
                            JSON_ATTRIBUTE_SHORT_KEY, error);
}

const char *get_avu_value(json_t *avu, baton_error_t *error) {
    return get_string_value(avu, "AVU", JSON_VALUE_KEY,
                            JSON_VALUE_SHORT_KEY, error);
}

char *make_in_op_value(json_t *avu, baton_error_t *error) {
    json_t *op_value = NULL;
    json_t *valarray = get_json_value(avu, "value", JSON_VALUE_KEY,
                                      JSON_VALUE_SHORT_KEY, error);
    if (error->code != 0) goto error;
    if (!json_is_array(valarray)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid 'value' attribute: not a JSON array "
                        "(required for `in` condition)");
        goto error;
    }

    json_t *prev_value;
    // Open paren
    op_value = json_string("(");

    size_t index;
    json_t *value;
    json_array_foreach(valarray, index, value) {
        if (!json_is_string(value)) {
            set_baton_error(error, CAT_INVALID_ARGUMENT,
                            "Invalid AVU value: not a JSON string "
                            "in item %d of `in` array", index);
            goto error;
        }

        prev_value = op_value;
        json_t *tmp;
        if (index == 0) {
            tmp = json_pack("s+++", json_string_value(prev_value),
                            "'", json_string_value(value),  "'");

        }
        else {
            tmp = json_pack("s+++", json_string_value(prev_value),
                            ", '", json_string_value(value), "'");
        }

        if (tmp) {
            op_value = tmp;
            json_decref(prev_value);
        }
    }

    // Close paren
    prev_value = op_value;
    json_t *tmp = json_pack("s+", json_string_value(op_value), ")");
    if (tmp) {
        op_value = tmp;
        json_decref(prev_value);
    }

    logmsg(DEBUG, "Using IN value of %s", json_string_value(op_value));

    char *copy = copy_str(json_string_value(op_value), MAX_STR_LEN);
    json_decref(op_value);

    return copy;

error:
    if (op_value) json_decref(op_value);

    return NULL;
}

const char *get_avu_units(json_t *avu, baton_error_t *error) {
    return get_opt_string_value(avu, "AVU", JSON_UNITS_KEY,
                                JSON_UNITS_SHORT_KEY, error);
}

const char *get_avu_operator(json_t *avu, baton_error_t *error) {
    return get_opt_string_value(avu, "AVU", JSON_OPERATOR_KEY,
                                JSON_OPERATOR_SHORT_KEY, error);
}

const char *get_access_owner(json_t *access, baton_error_t *error) {
    return get_string_value(access, "access spec", JSON_OWNER_KEY, NULL, error);
}

const char *get_access_level(json_t *access, baton_error_t *error) {
    return get_string_value(access, "access spec", JSON_LEVEL_KEY, NULL, error);
}

const char *get_timestamp_operator(json_t *timestamp, baton_error_t *error) {
    return get_opt_string_value(timestamp, "timestamp", JSON_OPERATOR_KEY,
                                JSON_OPERATOR_SHORT_KEY, error);
}

int has_collection(json_t *object) {
    baton_error_t error;

    init_baton_error(&error); // Ignore error

    return get_opt_string_value(object, "path spec", JSON_COLLECTION_KEY,
                                JSON_COLLECTION_SHORT_KEY, &error) != NULL;
}

int has_acl(json_t *object) {
    return json_object_get(object, JSON_ACCESS_KEY) != NULL;
}

int has_timestamps(json_t *object) {
  baton_error_t error;

  init_baton_error(&error); // Ignore error

  return get_timestamps(object, &error) != NULL;
}

int has_created_timestamp(json_t *object) {
  return has_json_str_value(object, JSON_CREATED_KEY, JSON_CREATED_SHORT_KEY);
}

int has_modified_timestamp(json_t *object) {
  return has_json_str_value(object, JSON_MODIFIED_KEY, JSON_MODIFIED_SHORT_KEY);
}

int contains_avu(json_t *avus, json_t *avu) {
    int has_avu = 0;

    size_t num_avus = json_array_size(avus);
    for (size_t i = 0; i < num_avus; i++) {
        json_t *x = json_array_get(avus, i);
        if (json_equal(x, avu)) {
            has_avu = 1;
            break;
        }
    }

    return has_avu;
}

int represents_collection(json_t *object) {
    return (has_json_str_value(object, JSON_COLLECTION_KEY,
                               JSON_COLLECTION_SHORT_KEY) &&
            !has_json_str_value(object, JSON_DATA_OBJECT_KEY,
                                JSON_DATA_OBJECT_SHORT_KEY));
}

int represents_data_object(json_t *object) {
    return (has_json_str_value(object, JSON_COLLECTION_KEY,
                               JSON_COLLECTION_SHORT_KEY) &&
            has_json_str_value(object, JSON_DATA_OBJECT_KEY,
                               JSON_DATA_OBJECT_SHORT_KEY));
}

int represents_directory(json_t *object) {
    return (has_json_str_value(object, JSON_DIRECTORY_KEY,
                               JSON_DIRECTORY_SHORT_KEY) &&
            !has_json_str_value(object, JSON_FILE_KEY, NULL));
}

int represents_file(json_t *object) {
    return (has_json_str_value(object, JSON_DIRECTORY_KEY,
                               JSON_DIRECTORY_SHORT_KEY) &&
            has_json_str_value(object, JSON_FILE_KEY, NULL));
}

json_t *make_timestamp(const char* key, const char *value, const char *format,
                       const char *repl_num, baton_error_t *error) {
    char *formatted = format_timestamp(value, format);

    json_t *result = json_pack("{s:s}", key, formatted);
    if (!result) {
        set_baton_error(error, -1,
                        "Failed to pack timestamp '%s': '%s' as JSON",
                        key, value);
        goto error;
    }

    if (repl_num) {
        int base = 10;
        char *endptr;
        int repl = strtoul(repl_num, &endptr, base);
        if (*endptr) {
            set_baton_error(error, -1,
                            "Failed to parse replicate number from "
                            "string '%s'", repl_num);
            goto error;
        }

        json_object_set_new(result, JSON_REPLICATE_KEY, json_integer(repl));
    }

    free(formatted);

    return result;

error:
    if (formatted) free(formatted);

    return NULL;
}

int add_timestamps(json_t *object, const char *created, const char *modified,
                   const char *repl_num, baton_error_t *error) {
    json_t *iso_created  = NULL;
    json_t *iso_modified = NULL;
    json_t *timestamps;

    if (!json_is_object(object)) {
        set_baton_error(error, -1, "Failed to add timestamp data: "
                        "target not a JSON object");
        goto error;
    }

    iso_created = make_timestamp(JSON_CREATED_KEY, created,
                                 ISO8601_FORMAT, repl_num, error);
    if (error->code != 0) goto error;

    iso_modified = make_timestamp(JSON_MODIFIED_KEY, modified,
                                  ISO8601_FORMAT, repl_num, error);
    if (error->code != 0) goto error;

    timestamps = json_pack("[o, o]", iso_created, iso_modified);
    if (!timestamps) {
        set_baton_error(error, -1, "Failed to pack timestamp array");
        goto error;
    }

    return json_object_set_new(object, JSON_TIMESTAMPS_KEY, timestamps);

error:
    return error->code;
}

int add_replicates(json_t *object, json_t *replicates, baton_error_t *error) {
    if (!json_is_object(object)) {
        set_baton_error(error, -1, "Failed to add replicates data: "
                        "target not a JSON object");
        goto error;
    }

    return json_object_set_new(object, JSON_REPLICATE_KEY, replicates);

error:
    return error->code;
}

int add_checksum(json_t *object, json_t *checksum, baton_error_t *error) {
    if (!json_is_object(object)) {
        set_baton_error(error, -1, "Failed to add checksum data: "
                        "target not a JSON object");
        goto error;
    }

    return json_object_set_new(object, JSON_CHECKSUM_KEY, checksum);

error:
    return error->code;
}

int add_collection(json_t *object, const char *coll_name,
                   baton_error_t *error) {
    if (!json_is_object(object)) {
        set_baton_error(error, -1, "Failed to add collection data: "
                        "target not a JSON object");
        goto error;
    }

    json_t *coll = json_pack("s", coll_name);
    if (!coll) {
        set_baton_error(error, -1, "Failed to pack data object '%s' as JSON",
                        coll_name);
        goto error;
    }

    return json_object_set_new(object, JSON_COLLECTION_KEY, coll);

error:
    return error->code;
}

int add_metadata(json_t *object, json_t *avus, baton_error_t *error) {
    if (!json_is_object(object)) {
        set_baton_error(error, -1, "Failed to add AVU data: "
                        "target not a JSON object");
        goto error;
    }

    return json_object_set_new(object, JSON_AVUS_KEY, avus);

error:
    return error->code;
}

int add_permissions(json_t *object, json_t *perms, baton_error_t *error) {
    if (!json_is_object(object)) {
        set_baton_error(error, -1, "Failed to add permissions data: "
                        "target not a JSON object");
        goto error;
    }

    return json_object_set_new(object, JSON_ACCESS_KEY, perms);

error:
    return error->code;
}

int add_contents(json_t *object, json_t *contents, baton_error_t *error) {
  if (!json_is_object(object)) {
      set_baton_error(error, -1, "Failed to add contents data: "
                      "target not a JSON object");
      goto error;
  }

  return json_object_set_new(object, JSON_CONTENTS_KEY, contents);

error:
    return error->code;
}

json_t *data_object_parts_to_json(const char *coll_name,
                                  const char *data_name,
                                  baton_error_t *error) {
    json_t *result = json_pack("{s:s, s:s}",
                               JSON_COLLECTION_KEY,  coll_name,
                               JSON_DATA_OBJECT_KEY, data_name);
    if (!result) {
        set_baton_error(error, -1, "Failed to pack data object '%s/%s' as JSON",
                        coll_name, data_name);
        goto error;
    }

    return result;

error:
    return NULL;
}

json_t *data_object_path_to_json(const char *path, baton_error_t *error) {
    char path1[MAX_STR_LEN];
    char path2[MAX_STR_LEN];
    char *coll_name;
    char *data_name;

    size_t term_len = strnlen(path, MAX_STR_LEN) + 1;
    if (term_len > MAX_STR_LEN) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Failed to pack the data object path '%s' as JSON: "
                        "it exceeded the maximum length of %d characters",
                        path, MAX_STR_LEN);
        goto error;
    }

    snprintf(path1, sizeof path1, "%s", path);
    coll_name = dirname(path1);
    if (!coll_name) {
        set_baton_error(error, errno,
                        "Failed to parse collection name of '%s': error %d %s",
                        path1, errno, strerror(errno));
        goto error;
    }

    snprintf(path2, sizeof path2, "%s", path);
    data_name = basename(path2);
    if (!data_name) {
        set_baton_error(error, errno,
                        "Failed to parse data object name of '%s': error %d %s",
                        path2, errno, strerror(errno));
        goto error;
    }

    json_t *result = data_object_parts_to_json(coll_name, data_name, error);
    if (error->code != 0) goto error;

    return result;

error:
    return NULL;
}

json_t *collection_path_to_json(const char *path, baton_error_t *error) {
    json_t *result = json_pack("{s:s}", JSON_COLLECTION_KEY, path);
    if (!result) {
        set_baton_error(error, -1, "Failed to pack collection '%s' as JSON",
                        path);
        goto error;
    }

    return result;

error:
    return NULL;
}

char *json_to_path(json_t *object, baton_error_t *error) {
    char *path = NULL;

    init_baton_error(error);

    const char *collection = get_collection_value(object, error);
    if (error->code != 0) goto error;

    if (represents_collection(object)) {
        path = make_dir_path(collection, error);
    }
    else {
        const char *data_object = get_data_object_value(object, error);
        path = make_file_path(collection, data_object, error);
    }

    if (error->code != 0) goto error;

    return path;

error:
    if (path) free(path);

    return NULL;
}

char *json_to_collection_path(json_t *object, baton_error_t *error) {
    char *path = NULL;

    init_baton_error(error);

    const char *collection = get_collection_value(object, error);
    if (error->code != 0) goto error;

    path = make_dir_path(collection, error);
    if (error->code != 0) goto error;

    return path;

error:
    if (path) free(path);

    return NULL;
}

char *json_to_local_path(json_t *object, baton_error_t *error) {
    char *path = NULL;

    init_baton_error(error);

    const char *directory = get_directory_value(object, error);
    if (error->code != 0) goto error;
    const char *filename = get_file_value(object, error);
    if (error->code != 0) goto error;
    const char *data_object = get_data_object_value(object, error);
    if (error->code != 0) goto error;

    if (directory && filename) {
        path = make_file_path(directory, filename, error);
    }
    else if (directory && data_object) {
        path = make_file_path(directory, data_object, error);
    }
    else if (filename) {
        path = make_file_path(".", filename, error);
    }
     else if (data_object) {
        path = make_file_path(".", data_object, error);
    }
    else if (directory) {
        path = make_dir_path(directory, error);
    }
    else {
        path = make_dir_path(".", error);
    }

    if (error->code != 0) goto error;

    return path;

error:
    if (path) free(path);

    return NULL;
}

void print_json_stream(json_t *json, FILE *stream) {
    char *json_str = json_dumps(json, JSON_INDENT(0));
    if (json_str) {
        fprintf(stream, "%s\n", json_str);
        free(json_str);
    }

    return;
}
void print_json(json_t *json) {
    print_json_stream(json, stdout);

    return;
}

int add_error_report(json_t *target, baton_error_t *error) {
    if (error->code != 0) {
        add_error_value(target, error);
    }

    return error->code;
}

static json_t *get_json_value(json_t *object, const char *name,
                              const char *key, const char *short_key,
                              baton_error_t *error) {
    json_t *value;

    if (!json_is_object(object)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid %s: not a JSON object", name);
        goto error;
    }

    value = json_object_get(object, key);
    if (!value && short_key) {
        value = json_object_get(object, short_key);
    }

    if (!value) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid iRODS %s: %s property is missing", name, key);
        goto error;
    }

    return value;

error:
    return NULL;
}

static const char *get_string_value(json_t *object, const char *name,
                                    const char *key, const char *short_key,
                                    baton_error_t *error) {
    json_t *value;

    if (!json_is_object(object)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid %s: not a JSON object", name);
        goto error;
    }

    value = json_object_get(object, key);
    if (!value && short_key) {
        value = json_object_get(object, short_key);
    }

    if (!value) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid iRODS %s: %s property is missing", name, key);
        goto error;
    }

    if (!json_is_string(value)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid %s %s: not a JSON string", name, key);
        goto error;
    }

    return json_string_value(value);

error:
    return NULL;
}

static const char *get_opt_string_value(json_t *object, const char *name,
                                        const char *key, const char *short_key,
                                        baton_error_t *error) {
    const char *str = NULL;
    json_t *value;
    if (!json_is_object(object)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid %s: not a JSON object", name);
        goto error;
    }

    value = json_object_get(object, key);
    if (!value && short_key) {
        value = json_object_get(object, short_key);
    }

    if (value && !json_is_string(value)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid %s %s: not a JSON string", name, key);
        goto error;
    }

    if (value) {
        str = json_string_value(value);
    }

    return str;

error:
    return NULL;
}

static int has_json_str_value(json_t *object, const char *key,
                              const char *short_key) {
    json_t *value = NULL;

    if (json_is_object(object)) {
        value = json_object_get(object, key);

        if (!value && short_key) {
            value = json_object_get(object, short_key);
        }
    }

    return value && json_is_string(value);
}

static char *make_dir_path(const char *path, baton_error_t *error) {
    size_t dlen = strnlen(path, MAX_STR_LEN) + 1; // +1 for NUL
    if (str_ends_with(path, "/", MAX_STR_LEN)) {
        dlen--;
    }

    if (dlen > MAX_STR_LEN) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "The path '%s' exceeded the maximum "
                        "length of %d characters", path, MAX_STR_LEN);
        goto error;
    }

    char *dpath = NULL;
    dpath = calloc(dlen, sizeof (char));
    if (!dpath) {
        set_baton_error(error, errno, "Failed to allocate memory: error %d %s",
                        errno, strerror(errno));
        goto error;
    }

    snprintf(dpath, dlen, "%s", path);

    return dpath;

error:
    return NULL;
}

static char *make_file_path(const char *path, const char *filename,
                            baton_error_t *error) {
    size_t dlen = strnlen(path, MAX_STR_LEN);
    size_t flen = strnlen(filename, MAX_STR_LEN);
    size_t len = dlen + flen + 1; // +1 for NUL

    int includes_slash = str_ends_with(path, "/", MAX_STR_LEN);
    if (!includes_slash) len++;

    if (len > MAX_STR_LEN) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "The path components '%s' + '%s' "
                        "combined exceeded the maximum length of %d "
                        "characters", path, filename, MAX_STR_LEN);
        goto error;
    }

    char *fpath = NULL;
    fpath = calloc(len, sizeof (char));
    if (!fpath) {
        set_baton_error(error, errno,
                        "Failed to allocate memory: error %d %s",
                        errno, strerror(errno));
        goto error;
    }

    if (includes_slash) {
        snprintf(fpath, len, "%s%s", path, filename);
    }
    else {
        snprintf(fpath, len, "%s/%s", path, filename);
    }

    return fpath;

error:
    return NULL;
}
