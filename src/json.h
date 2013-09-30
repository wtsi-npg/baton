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

#ifndef _BATON_JSON_H
#define _BATON_JSON_H

#define JSON_DATA_OBJECT_KEY "data_object"
#define JSON_COLLECTION_KEY "collection"

json_t *data_object_path_to_json(const char *path);

json_t *collection_path_to_json(const char *path);

char *json_to_path(json_t *json);

void print_json(json_t* results);

#endif // _BATON_JSON_H
