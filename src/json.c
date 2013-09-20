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
 * @author Keith James <kdj@sanger.ac.uk>
 */

#include <assert.h>
#include <libgen.h>
#include <string.h>
#include <stdlib.h>

#include <jansson.h>

#include "utilities.h"

json_t *data_object_path_to_json(const char *path) {
    size_t len = strlen(path) + 1;

    char *path1 = calloc(len, sizeof (char));
    char *path2 = calloc(len, sizeof (char));
    strncpy(path1, path, len);
    strncpy(path2, path, len);

    char *coll_name = dirname(path1);
    char *data_name = basename(path2);

    json_t *result = json_pack("{s:s, s:s}",
                               "collection", coll_name,
                               "data_object", data_name);
    free(path1);
    free(path2);

    return result;
}

json_t *collection_path_to_json(const char *path) {
    return json_pack("{s:s}", "collection", path);
}

char *json_to_path(json_t *json) {
    assert(json);
    char *path;

    json_t *coll = json_object_get(json, "collection");
    json_t *data = json_object_get(json, "data_object");
    if (!coll) {
        logmsg(ERROR, BATON_CAT, "collection value was missing");
        goto error;
    }
    if (!json_is_string(coll)) {
        logmsg(ERROR, BATON_CAT, "collection value was not a string");
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

        if (ends_with(collection, "/")) {
            path = calloc(len, sizeof (char));
            snprintf(path, len, "%s%s", collection, data_object);
        }
        else {
            path = calloc(len + 1, sizeof (char));
            snprintf(path, len + 1, "%s/%s", collection, data_object);
        }
    }

    return path;

error:
    return NULL;
}

void print_json(json_t *json) {
    char *json_str = json_dumps(json, JSON_INDENT(1));
    printf("%s\n", json_str);
    free(json_str);

    return;
}
