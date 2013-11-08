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

int do_supersede_metadata(int argc, char *argv[], int optind, FILE *input);

int main(int argc, char *argv[]) {
    int exit_status = 0;
    char *json_file = NULL;
    FILE *input = NULL;

    while (1) {
        static struct option long_options[] = {
            // Flag options
            {"help",      no_argument, &help_flag,    1},
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
        puts("    json-metasuper");
        puts("");
        puts("Synopsis");
        puts("");
        puts("    json-metasuper [--file <json file>]");
        puts("");
        puts("Description");
        puts("    Supersedes metadata AVUs on collections and data objects");
        puts("described in a JSON input file.");
        puts("");
        puts("    --file        The JSON file describing the data objects.");
        puts("                  Optional, defaults to STDIN.");
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

    input = maybe_stdin(json_file);
    int status = do_supersede_metadata(argc, argv, optind, input);

    if (status != 0) exit_status = 5;

    zlog_fini();
    exit(exit_status);

args_error:
    exit_status = 4;

error:
    zlog_fini();
    exit(exit_status);
}

int do_supersede_metadata(int argc, char *argv[], int optind, FILE *input) {
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
        }
        else {
            char *path = json_to_path(target);
            path_count++;

            json_t *avus = json_object_get(target, JSON_AVUS_KEY);
            if (!json_is_array(avus)) {
                logmsg(ERROR, BATON_CAT,
                       "AVU data for '%s' is not in a JSON array", path);
                goto error;
            }

            rodsPath_t rods_path;
            int status = resolve_rods_path(conn, &env, &rods_path, path);
            if (status < 0) {
               error_count++;
               logmsg(ERROR, BATON_CAT, "Failed to resolve path '%s'", path);
            }
            else {
                baton_error_t list_error;
                json_t *current_avus = list_metadata(conn, &rods_path, NULL,
                                                     &list_error);
                if (list_error.code != 0) {
                    error_count++;
                    add_error_value(target, &list_error);
                    goto print_result;
                }

                // Remove any current AVUs that are not equal to target AVUs
                for (size_t i = 0; i < json_array_size(current_avus); i++) {
                    json_t *current_avu = json_array_get(current_avus, i);
                    char *str = json_dumps(current_avu, JSON_DECODE_ANY);

                    if (contains_avu(avus, current_avu)) {
                        logmsg(TRACE, BATON_CAT, "Not removing AVU %s", str);
                    }
                    else {
                        baton_error_t rem_error;
                        logmsg(TRACE, BATON_CAT, "Removing AVU %s", str);
                        modify_json_metadata(conn, &rods_path, META_REM,
                                             current_avu, &rem_error);
                        if (rem_error.code != 0) {
                            error_count++;
                            add_error_value(target, &rem_error);
                            free(str);
                            goto print_result;
                        }
                    }

                    free(str);
                }

                // Add any target AVUs that are not equal to current AVUs
                for (size_t i = 0; i < json_array_size(avus); i++) {
                    json_t *avu = json_array_get(avus, i);
                    char *str = json_dumps(avu, JSON_DECODE_ANY);

                    if (contains_avu(current_avus, avu)) {
                        logmsg(TRACE, BATON_CAT, "Not adding AVU %s", str);
                    }
                    else {
                        baton_error_t add_error;
                        logmsg(TRACE, BATON_CAT, "Adding AVU %s", str);
                        modify_json_metadata(conn, &rods_path, META_ADD, avu,
                                             &add_error);
                        if (add_error.code != 0) {
                            error_count++;
                            add_error_value(target, &add_error);
                            free(str);
                            goto print_result;
                        }
                    }

                    free(str);
                }

                json_decref(current_avus);
            }

        print_result:
            print_json(target);
            fflush(stdout);
            json_decref(target);
            free(path);
        }
    }

    rcDisconnect(conn);

    logmsg(TRACE, BATON_CAT, "Processed %d paths with %d errors",
           path_count, error_count);

    return error_count;

error:
    if (conn) rcDisconnect(conn);

    logmsg(ERROR, BATON_CAT, "Processed %d paths with %d errors",
           path_count, error_count);

    return 1;
}
