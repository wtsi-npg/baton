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

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "rodsClient.h"
#include "rodsPath.h"
#include <jansson.h>
#include <zlog.h>

#include "baton.h"
#include "config.h"
#include "json.h"
#include "utilities.h"

static char *SYSTEM_LOG_CONF_FILE = ZLOG_CONF;

static char *USER_LOG_CONF_FILE = NULL;

static int help_flag;
static int version_flag;

int do_modify_metadata(int argc, char *argv[], int optind,
                       metadata_op operation, const char *json_file);

int main(int argc, char *argv[]) {
    int exit_status;
    metadata_op meta_op = -1;
    char *json_file = NULL;

    while (1) {
        static struct option long_options[] = {
            // Flag options
            {"help",      no_argument, &help_flag,    1},
            {"version",   no_argument, &version_flag, 1},
            // Indexed options
            {"file",      required_argument, NULL, 'f'},
            {"logconf",   required_argument, NULL, 'l'},
            {"operation", required_argument, NULL, 'o'},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        int c = getopt_long_only(argc, argv, "f:l:o:",
                                 long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1) break;

        switch (c) {
            case 'f':
                json_file = optarg;
                break;

            case 'l':
                USER_LOG_CONF_FILE = optarg;
                break;

            case 'o':
                if (strcmp("add", optarg) ==  0) {
                    meta_op = META_ADD;
                }

                break;

            case '?':
                // getopt_long already printed an error message
                break;

            default:
                // Ignore
                break;
        }
    }

    if (help_flag) {
        puts("Name");
        puts("    json-metamod");
        puts("");
        puts("Synopsis");
        puts("");
        puts("    json-metamod -o <operation> --file <json file>");
        puts("");
        puts("Description");
        puts("    Modifies metadata AVUs on collections and data objects");
        puts("described in a JSON input file.");
        puts("");
        puts("    --operation   Operation to perform. One of [add]. Required.");
        puts("    --file        The JSON file describing the data objects.");
        puts("                  Required");
        puts("");

        exit(0);
    }

    if (version_flag) {
        printf("%s\n", VERSION);
        exit(0);
    }

    if (USER_LOG_CONF_FILE == NULL) {
        if (zlog_init(SYSTEM_LOG_CONF_FILE)) {
            fprintf(stderr, "Logging configuration failed "
                    "(using system-defined configuration in '%s')\n",
                    SYSTEM_LOG_CONF_FILE);
        }
    }
    else {
        if (zlog_init(USER_LOG_CONF_FILE)) {
            fprintf(stderr, "Logging configuration failed "
                    "(using user-defined configuration in '%s')\n",
                    USER_LOG_CONF_FILE);
        }
    }

    switch (meta_op) {
        case META_ADD:
            if (!json_file) {
                fprintf(stderr, "A --file argument is required\n");
                goto args_error;
            }

            int status = do_modify_metadata(argc, argv, optind, meta_op,
                                            json_file);
            if (status != 0) {
                exit_status = 5;
            }

            break;

        default:
            fprintf(stderr, "No valid operation was specified; valid "
                    "operations are: [add]\n");
            goto args_error;
    }

    zlog_fini();
    exit(exit_status);

args_error:
    exit_status = 4;

error:
    zlog_fini();
    exit(exit_status);
}

int do_modify_metadata(int argc, char *argv[], int optind,
                       metadata_op operation, const char *json_file) {
    char *err_name;
    char *err_subname;

    int path_count = 0;
    int error_count = 0;

    rodsEnv env;
    rodsPath_t rods_path;
    rcComm_t *conn = rods_login(&env);
    if (!conn) goto error;

    json_error_t error;
    json_t *targets = json_load_file(json_file, JSON_REJECT_DUPLICATES, &error);
    if (!targets) goto json_error;

    for (size_t i = 0; i < json_array_size(targets); i++) {
        json_t *target = json_array_get(targets, i);
        char *path = json_to_path(target);
        path_count++;

        json_t *avus = json_object_get(target, "avus");
        if (!json_is_array(avus)) {
            logmsg(ERROR, BATON_CAT,
                   "AVU data for %s is not in a JSON array", path);
            goto error;
        }

        int status = resolve_rods_path(conn, &env, &rods_path, path);
        if (status < 0) {
            error_count++;
            logmsg(ERROR, BATON_CAT, "Failed to resolve path '%s'", path);
        }
        else {
            for (size_t j = 0; j < json_array_size(avus); j++) {
                json_t *avu = json_array_get(avus, j);

                char *attr_name = NULL;
                char *attr_value = NULL;
                char *attr_units = "";

                const char *key;
                json_t *value;
                json_object_foreach(avu, key, value) {
                    if ((strcmp(key, "attribute") == 0)) {
                        attr_name = copy_str(json_string_value(value));
                    }
                    else if ((strcmp(key, "value") == 0)) {
                        attr_value = copy_str(json_string_value(value));
                    }
                    else if ((strcmp(key, "units") == 0)) {
                        attr_units = copy_str(json_string_value(value));
                    }
                }

                status = modify_metadata(conn, &rods_path, operation,
                                         attr_name, attr_value, attr_units);
                if (status < 0) {
                    error_count++;
                    err_name = rodsErrorName(status, &err_subname);
                    logmsg(ERROR, BATON_CAT,
                           "Failed to add metadata ['%s' '%s' '%s'] to '%s': "
                           "error %d %s %s",
                           attr_name, attr_value, attr_units, rods_path.outPath,
                           status, err_name, err_subname);
                }
            }
        }
    }

    rcDisconnect(conn);

    logmsg(DEBUG, BATON_CAT, "Processed %d paths with %d errors",
           path_count, error_count);

    return error_count;

json_error:
    logmsg(ERROR, BATON_CAT, "JSON error at line %d, column %d: %s",
           error.line, error.column, error.text);

error:
    if (conn) rcDisconnect(conn);

    logmsg(ERROR, BATON_CAT, "Processed %d paths with %d errors",
           path_count, error_count);

    return 1;
}
