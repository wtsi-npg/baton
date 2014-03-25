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
 * @file json.c
 * @author Keith James <kdj@sanger.ac.uk>
 */

#include <assert.h>
#include <libgen.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <jansson.h>

#include "json.h"
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

json_t *error_to_json(baton_error_t *error) {
    json_t *err = json_pack("{s:s, s:i}",
                            JSON_ERROR_MSG_KEY , error->message,
                            JSON_ERROR_CODE_KEY, error->code);

    if (!err) {
        logmsg(ERROR, BATON_CAT, "Failed to pack error '%s' as JSON",
               error->message);
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
    json_t *timestamps = get_json_value(object, "path spec", JSON_TIMESTAMP_KEY,
                                        NULL, error);
    if (!json_is_array(timestamps)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid '%s' attribute: not a JSON array",
                        JSON_TIMESTAMP_KEY);
        goto error;
    }

    return timestamps;

error:
    return NULL;
}

const char* get_collection_value(json_t *object, baton_error_t *error) {
    return get_string_value(object, "path spec", JSON_COLLECTION_KEY,
                            JSON_COLLECTION_SHORT_KEY, error);
}

const char* get_data_object_value(json_t *object, baton_error_t *error) {
    return get_opt_string_value(object, "path spec", JSON_DATA_OBJECT_KEY,
                                JSON_DATA_OBJECT_SHORT_KEY, error);
}

const char* get_created_timestamp(json_t *object, baton_error_t *error) {
    return get_string_value(object, "path spec", JSON_CREATED_KEY,
                            JSON_CREATED_SHORT_KEY, error);
}

const char* get_modified_timestamp(json_t *object, baton_error_t *error) {
    return get_string_value(object, "path spec", JSON_MODIFIED_KEY,
                            JSON_MODIFIED_SHORT_KEY, error);
}

const char *get_avu_attribute(json_t *avu, baton_error_t *error) {
    return get_string_value(avu, "AVU", JSON_ATTRIBUTE_KEY,
                            JSON_ATTRIBUTE_SHORT_KEY, error);
}

const char *get_avu_value(json_t *avu, baton_error_t *error) {
    return get_string_value(avu, "AVU", JSON_VALUE_KEY,
                            JSON_VALUE_SHORT_KEY, error);
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

int has_acl(json_t *object) {
    return json_object_get(object, JSON_ACCESS_KEY) != NULL;
}

int has_timestamps(json_t *object) {
    return json_object_get(object, JSON_TIMESTAMP_KEY) != NULL;
}

int has_created_timestamp(json_t *object) {
    return json_object_get(object, JSON_CREATED_KEY) != NULL;
}

int has_modified_timestamp(json_t *object) {
    return json_object_get(object, JSON_MODIFIED_KEY) != NULL;
}

int contains_avu(json_t *avus, json_t *avu) {
    int has_avu = 0;

    for (size_t i = 0; i < json_array_size(avus); i++) {
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

json_t *make_timestamp(const char* key, const char *value, const char *format,
                       baton_error_t *error) {
    char *formatted = format_timestamp(value, format);

    json_t *result = json_pack("{s:s}", key, formatted);
    if (!result) {
        set_baton_error(error, -1,
                        "Failed to pack timestamp '%s': '%s' as JSON",
                        key, value);
        goto error;
    }

    free(formatted);

    return result;

error:
    if (formatted) free(formatted);

    return NULL;
}

int add_timestamps(json_t *object, const char *created, const char *modified,
                   baton_error_t *error) {
    json_t *iso_created  = NULL;
    json_t *iso_modified = NULL;

    if (!json_is_object(object)) {
        set_baton_error(error, -1, "Failed to add timestamp data: "
                        "target not a JSON object");
        goto error;
    }

    iso_created = make_timestamp(JSON_CREATED_KEY, created,
                                 ISO8601_FORMAT, error);
    if (error->code != 0) goto error;

    iso_modified = make_timestamp(JSON_MODIFIED_KEY, modified,
                                  ISO8601_FORMAT, error);
    if (error->code != 0) goto error;

    json_t *timestamps = json_pack("[o, o]", iso_created, iso_modified);
    if (!timestamps) {
        logmsg(ERROR, BATON_CAT, "Failed to pack timestamp array");
    }

    return json_object_set_new(object, JSON_TIMESTAMP_KEY, timestamps);

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

json_t *data_object_parts_to_json(const char *coll_name,
                                  const char *data_name) {
    json_t *result = json_pack("{s:s, s:s}",
                               JSON_COLLECTION_KEY,  coll_name,
                               JSON_DATA_OBJECT_KEY, data_name);
    if (!result) {
        logmsg(ERROR, BATON_CAT, "Failed to pack data object '%s/%s' as JSON",
               coll_name, data_name);
    }

    return result;
}

json_t *data_object_path_to_json(const char *path) {
    size_t len = strlen(path) + 1;

    char *path1 = calloc(len, sizeof (char));
    char *path2 = calloc(len, sizeof (char));
    strncpy(path1, path, len);
    strncpy(path2, path, len);

    char *coll_name = dirname(path1);
    char *data_name = basename(path2);

    json_t *result = data_object_parts_to_json(coll_name, data_name);
    free(path1);
    free(path2);

    return result;
}

json_t *query_args_to_json(const char *attr_name, const char *attr_value,
                           const char *root_path) {
    json_t *result;
    if (root_path) {
        result = json_pack("{s:s, s:[{s:s, s:s}]}",
                           JSON_COLLECTION_KEY, root_path,
                           JSON_AVUS_KEY,
                           JSON_ATTRIBUTE_KEY, attr_name,
                           JSON_VALUE_KEY,     attr_value);
    }
    else {
        result = json_pack("{s:[{s:s, s:s}]}",
                           JSON_AVUS_KEY,
                           JSON_ATTRIBUTE_KEY, attr_name,
                           JSON_VALUE_KEY,     attr_value);
    }

    if (!result) {
        logmsg(ERROR, BATON_CAT, "Failed to pack query attribute: '%s', "
               "value: '%s', path: '%s' as JSON", attr_name, attr_value,
               root_path);
    }

    return result;
}

json_t *collection_path_to_json(const char *path) {
    json_t *result = json_pack("{s:s}", JSON_COLLECTION_KEY, path);
    if (!result) {
        logmsg(ERROR, BATON_CAT, "Failed to pack collection '%s' as JSON",
               path);
    }

    return result;
}

char *json_to_path(json_t *object, baton_error_t *error) {
    char *path;
    init_baton_error(error);

    const char *collection = get_collection_value(object, error);
    if (error->code != 0) goto error;

    if (!represents_data_object(object)) {
        path = copy_str(collection);
    }
    else {
        const char *data_object = get_data_object_value(object, error);

        size_t clen = strlen(collection);
        size_t dlen = strlen(data_object);
        size_t len = clen + dlen + 1;

        if (str_ends_with(collection, "/")) {
            path = calloc(len, sizeof (char));
            if (!path) {
                set_baton_error(error, errno,
                                "Failed to allocate memory: error %d %s",
                                errno, strerror(errno));
                goto error;
            }

            snprintf(path, len, "%s%s", collection, data_object);
        }
        else {
            path = calloc(len + 1, sizeof (char));
            if (!path) {
                set_baton_error(error, errno,
                                "Failed to allocate memory: error %d %s",
                                errno, strerror(errno));
                goto error;
            }

            snprintf(path, len + 1, "%s/%s", collection, data_object);
        }
    }

    return path;

error:
    return NULL;
}

void print_json(json_t *json) {
    char *json_str = json_dumps(json, JSON_INDENT(0));
    printf("%s\n", json_str);
    free(json_str);

    return;
}

static json_t *get_json_value(json_t *object, const char *name,
                              const char *key, const char *short_key,
                              baton_error_t *error) {
    if (!json_is_object(object)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid %s: not a JSON object", name);
        goto error;
    }

    json_t *value = json_object_get(object, key);
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
    if (!json_is_object(object)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid %s: not a JSON object", name);
        goto error;
    }

    json_t *value = json_object_get(object, key);
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
    if (!json_is_object(object)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid %s: not a JSON object", name);
        goto error;
    }

    const char *str = NULL;

    json_t *value = json_object_get(object, key);
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
