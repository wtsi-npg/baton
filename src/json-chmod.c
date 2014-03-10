/**
 * Copyright (c) 2014 Genome Research Ltd. All rights reserved.
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

static char *SYSTEM_LOG_CONF_FILE = ZLOG_CONF; // Set by autoconf

static char *USER_LOG_CONF_FILE = NULL;

static int help_flag;
static int recurse_flag;
static int version_flag;

int do_modify_permissions(int argc, char *argv[], int optind,
                          recursive_op recurse, FILE *input);

int main(int argc, char *argv[]) {
    int exit_status = 0;
    char *json_file = NULL;
    FILE *input = NULL;

    while (1) {
        static struct option long_options[] = {
            // Flag options
            {"help",      no_argument, &help_flag,    1},
            {"recurse",   no_argument, &recurse_flag, 1},
            {"version",   no_argument, &version_flag, 1},
            // Indexed options
            {"file",      required_argument, NULL, 'f'},
            {"logconf",   required_argument, NULL, 'l'},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        int c = getopt_long_only(argc, argv, "f:l:",
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
        puts("    json-chmod");
        puts("");
        puts("Synopsis");
        puts("");
        puts("    json-chmod [--file <json file>]");
        puts("");
        puts("Description");
        puts("    Set permissions on collections and data objects");
        puts("described in a JSON input file.");
        puts("");
        puts("    --file        The JSON file describing the data objects.");
        puts("                  Optional, defaults to STDIN.");
        puts("");
        puts("    --recurse     Modify collection permissions recursively.");
        puts("                  Optional, defaults to false.");
        puts("");

        exit(0);
    }

    if (version_flag) {
        printf("%s\n", VERSION);
        exit(0);
    }

    if (!USER_LOG_CONF_FILE) {
        if (zlog_init(SYSTEM_LOG_CONF_FILE)) {
            fprintf(stderr, "Logging configuration failed "
                    "(using system-defined configuration in '%s')\n",
                    SYSTEM_LOG_CONF_FILE);
        }
    }
    else if (zlog_init(USER_LOG_CONF_FILE)) {
        fprintf(stderr, "Logging configuration failed "
                "(using user-defined configuration in '%s')\n",
                USER_LOG_CONF_FILE);
    }

    declare_client_name(argv[0]);

    input = maybe_stdin(json_file);
    int status;

    if (recurse_flag) {
        status = do_modify_permissions(argc, argv, optind, RECURSE, input);
    }
    else {
        status = do_modify_permissions(argc, argv, optind, NO_RECURSE, input);
    }

    if (status != 0) exit_status = 5;

    zlog_fini();
    exit(exit_status);

args_error:
    exit_status = 4;

error:
    zlog_fini();
    exit(exit_status);
}

int do_modify_permissions(int argc, char *argv[], int optind,
                          recursive_op recurse, FILE *input) {
    int path_count = 0;
    int error_count = 0;

    rodsEnv env;
    rcComm_t *conn = rods_login(&env);
    if (!conn) goto error;

    size_t flags = JSON_DISABLE_EOF_CHECK | JSON_REJECT_DUPLICATES;

    while (!feof(input)) {
        json_error_t load_error;
        json_t *target = json_loadf(input, flags, &load_error);
        if (!target) {
            if (!feof(input)) {
                logmsg(ERROR, BATON_CAT, "JSON error at line %d, column %d: %s",
                       load_error.line, load_error.column, load_error.text);
            }

            continue;
        }

        baton_error_t path_error;
        char *path = json_to_path(target, &path_error);
        path_count++;

        if (path_error.code != 0) {
            error_count++;
            add_error_value(target, &path_error);
        }
        else {
            json_t *perms = json_object_get(target, JSON_ACCESS_KEY);
            if (perms) {
                if (!json_is_array(perms)) {
                    logmsg(ERROR, BATON_CAT,
                           "Permissions data for %s is not in a JSON array",
                           path);
                    goto error;
                }

                rodsPath_t rods_path;
                int status = resolve_rods_path(conn, &env, &rods_path, path);
                if (status < 0) {
                    error_count++;
                    logmsg(ERROR, BATON_CAT, "Failed to resolve path '%s'",
                           path);
                }
                else {
                    for (size_t i = 0; i < json_array_size(perms); i++) {
                        json_t *perm = json_array_get(perms, i);
                        baton_error_t mod_error;
                        modify_json_permissions(conn, &rods_path, recurse, perm,
                                                &mod_error);

                        if (mod_error.code != 0) {
                            error_count++;
                            add_error_value(target, &mod_error);
                        }
                    }
                }

                if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);
            }
        }

        print_json(target);
        fflush(stdout);
        json_decref(target);
        free(path);
    } // while

    rcDisconnect(conn);

    logmsg(DEBUG, BATON_CAT, "Processed %d paths with %d errors",
           path_count, error_count);

    return error_count;

error:
    if (conn) rcDisconnect(conn);

    logmsg(ERROR, BATON_CAT, "Processed %d paths with %d errors",
           path_count, error_count);

    return 1;
}
