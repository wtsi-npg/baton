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
 * @file json.h
 * @author Keith James <kdj@sanger.ac.uk>
 */

#ifndef _BATON_JSON_H
#define _BATON_JSON_H

#include "baton.h"

#define JSON_DATA_OBJECT_KEY "data_object"
#define JSON_COLLECTION_KEY  "collection"

#define JSON_ERROR_KEY       "error"
#define JSON_ERROR_CODE_KEY  "code"
#define JSON_ERROR_MSG_KEY   "message"

int add_error_value(json_t *object, baton_error_t *error);

json_t *error_to_json(baton_error_t *error);

int contains_avu(json_t *avus, json_t *avu);

int represents_collection(json_t *object);

int represents_data_object(json_t *object);

int add_permissions(json_t *object, json_t *perms, baton_error_t *error);

json_t *data_object_parts_to_json(const char *coll_name, const char *data_name);

json_t *data_object_path_to_json(const char *path);

json_t *collection_path_to_json(const char *path);

json_t *query_args_to_json(const char *attr_name, const char *attr_value,
                           const char *root_path);

char *json_to_path(json_t *object, baton_error_t *error);

void print_json(json_t *json);

#endif // _BATON_JSON_H
