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

#include <jansson.h>

#include "json.h"
#include "utilities.h"

static int has_string_value(json_t *json, const char *key);

json_t *error_to_json(baton_error_t *error) {
    return json_pack("{s:s, s:i}",
                     JSON_ERROR_MSG_KEY , error->message,
                     JSON_ERROR_CODE_KEY, error->code);
}

int add_error_value(json_t *object, baton_error_t *error) {
    json_t *err = error_to_json(error);
    return json_object_set_new(object, JSON_ERROR_KEY, err);
}

json_t *get_avus(json_t *object, baton_error_t *error) {
    if (!json_is_object(object)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid iRODS path spec: not a JSON object");
        goto error;
    }

    json_t *avus = json_object_get(object, JSON_AVUS_KEY);

    if (!avus) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid iRODS path spec: avus property is missing");
        goto error;
    }

    if (!json_is_array(avus)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid avus attribute: not a JSON array");
        goto error;
    }

    return avus;

error:
    return NULL;
}

const char *get_avu_attribute(json_t *avu, baton_error_t *error) {
    if (!json_is_object(avu)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid AVU: not a JSON object");
        goto error;
    }

    json_t *attr = json_object_get(avu, JSON_ATTRIBUTE_KEY);
    if (!attr) attr = json_object_get(avu, JSON_ATTRIBUTE_SHORT_KEY);

    if (!attr) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid AVU: attribute property is missing");
        goto error;
    }

    if (!json_is_string(attr)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid AVU attribute: not a JSON string");
        goto error;
    }

    return json_string_value(attr);

error:
    return NULL;
}

const char *get_avu_value(json_t *avu, baton_error_t *error) {
    if (!json_is_object(avu)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid AVU: not a JSON object");
        goto error;
    }

    json_t *value = json_object_get(avu, JSON_VALUE_KEY);
    if (!value) value = json_object_get(avu, JSON_VALUE_SHORT_KEY);

    if (!value) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid AVU: value property is missing");
        goto error;
    }

    if (!json_is_string(value)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid AVU value: not a JSON string");
        goto error;
    }

    return json_string_value(value);

error:
    return NULL;
}

const char *get_avu_units(json_t *avu, baton_error_t *error) {
    if (!json_is_object(avu)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid AVU: not a JSON object");
        goto error;
    }

    const char* units_str = NULL;

    json_t *units = json_object_get(avu, JSON_UNITS_KEY);
    if (!units) units = json_object_get(avu, JSON_UNITS_SHORT_KEY);

    if (units && !json_is_string(units)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid AVU units: not a JSON string");
        goto error;
    }

    if (units) {
        units_str = json_string_value(units);
    }

    return units_str;

error:
    return NULL;
}

const char *get_avu_operator(json_t *avu, baton_error_t *error) {
    if (!json_is_object(avu)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid AVU: not a JSON object");
        goto error;
    }

    const char* oper_str = NULL;

    json_t *oper = json_object_get(avu, JSON_OPERATOR_KEY);
    if (!oper) oper = json_object_get(avu, JSON_OPERATOR_SHORT_KEY);

    if (oper && !json_is_string(oper)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid AVU operator: not a JSON string");
        goto error;
    }

    if (oper) {
        oper_str = json_string_value(oper);
    }

    return oper_str;

error:
    return NULL;
}

const char *get_access_owner(json_t *access, baton_error_t *error) {
    if (!json_is_object(access)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid permissions specification: not a JSON object");
        goto error;
    }

    json_t *owner = json_object_get(access, JSON_OWNER_KEY);

    if (!owner) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid access: owner property is missing");
        goto error;
    }
    if (!json_is_string(owner)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid owner: not a JSON string");
        goto error;
    }

    return json_string_value(owner);

error:
    return NULL;
}

const char *get_access_level(json_t *access, baton_error_t *error) {
    if (!json_is_object(access)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid permissions specification: not a JSON object");
        goto error;
    }

    json_t *level = json_object_get(access, JSON_LEVEL_KEY);

    if (!level) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid access: level property is missing");
        goto error;
    }
    if (!json_is_string(level)) {
        set_baton_error(error, CAT_INVALID_ARGUMENT,
                        "Invalid level: not a JSON string");
        goto error;
    }

    return json_string_value(level);

error:
    return NULL;
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
    return (has_string_value(object, JSON_COLLECTION_KEY) &&
            !has_string_value(object, JSON_DATA_OBJECT_KEY));
}

int represents_data_object(json_t *object) {
    return (has_string_value(object, JSON_COLLECTION_KEY) &&
            has_string_value(object, JSON_DATA_OBJECT_KEY));
}

int add_metadata(json_t *object, json_t *avus, baton_error_t *error) {
    if (!json_is_object(object)) {
        set_baton_error(error, -1, "Failed to add AVU data: "
                        "target not a JSON object");
        goto error;
    }

    return json_object_set_new(object, JSON_AVUS_KEY, avus);

error:
    logmsg(ERROR, BATON_CAT, error->message);

    return -1;
}

int add_permissions(json_t *object, json_t *perms, baton_error_t *error) {
    if (!json_is_object(object)) {
        set_baton_error(error, -1, "Failed to add permissions data: "
                        "target not a JSON object");
        goto error;
    }

    return json_object_set_new(object, JSON_ACCESS_KEY, perms);

error:
    logmsg(ERROR, BATON_CAT, error->message);

    return -1;
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

    json_t *coll = json_object_get(object, JSON_COLLECTION_KEY);
    json_t *data = json_object_get(object, JSON_DATA_OBJECT_KEY);
    if (!coll) {
        set_baton_error(error, -1, "collection value was missing");
        goto error;
    }
    if (!json_is_string(coll)) {
        set_baton_error(error, -1,  "collection value was not a string");
        goto error;
    }

    const char *collection = json_string_value(coll);

    if (!data) {
        path = copy_str(collection);
    }
    else {
        const char *data_object = json_string_value(data);
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
    logmsg(ERROR, BATON_CAT, error->message);

    return NULL;
}

void print_json(json_t *json) {
    char *json_str = json_dumps(json, JSON_INDENT(0));
    printf("%s\n", json_str);
    free(json_str);

    return;
}

static int has_string_value(json_t *json, const char *key) {
    int result = 0;

    if (json_is_object(json)) {
        json_t *value = json_object_get(json, key);
        if (json_is_string(value)) result = 1;
    }

    return result;
}
