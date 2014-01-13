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
 * @author Keith James <kdj@sanger.ac.uk>
 */

#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "rodsClient.h"
#include "rodsPath.h"
#include <zlog.h>

#include "baton.h"
#include "config.h"
#include "json.h"

static char *SYSTEM_LOG_CONF_FILE = ZLOG_CONF;
static char *USER_LOG_CONF_FILE = NULL;

static int help_flag;
static int version_flag;

int do_search_metadata(int argc, char *argv[], int optind, FILE *input,
                       char *zone_name);

int main(int argc, char *argv[]) {
    int exit_status = 0;
    char *zone_name = NULL;
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
            {"zone",      required_argument, NULL, 'z'},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        int c = getopt_long_only(argc, argv, "f:l:z:",
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

            case 'z':
                zone_name = optarg;
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
        puts("    json-metaquery");
        puts("");
        puts("Synopsis");
        puts("");
        puts("    json-metaquery [--file <JSON file>] [--zone <name>]");
        puts("");
        puts("Description");
        puts("    Finds items in iRODS by AVU, given a query constructed");
        puts("from a JSON input file.");
        puts("");
        puts("    --file        The JSON file describing the query. Optional,");
        puts("                  defaults to STDIN.");
        puts("    --zone        The zone to search. Optional");
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
    int status = do_search_metadata(argc, argv, optind, input, zone_name);
    if (status != 0) exit_status = 5;

    zlog_fini();
    exit(exit_status);

args_error:
    exit_status = 4;

error:
    zlog_fini();
    exit(exit_status);
}

int do_search_metadata(int argc, char *argv[], int optind, FILE *input,
                       char *zone_name) {
    int query_count = 0;
    int error_count = 0;

    char *err_name;
    char *err_subname;

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

        baton_error_t error;
        json_t *results = search_metadata(conn, target, zone_name, &error);
        if (error.code != 0) {
            logmsg(ERROR, BATON_CAT, "Failed to search: %s", error.message);
            goto error;
        }
        else {
            for (size_t i = 0; i < json_array_size(results); i++) {
                json_t *result = json_array_get(results, i);

                baton_error_t path_error;
                char *path = json_to_path(result, &path_error);
                if (path_error.code != 0) {
                    error_count++;
                    logmsg(ERROR, BATON_CAT, path_error.message);
                }
                else {
                    rodsPath_t rods_path;
                    int status = resolve_rods_path(conn, &env, &rods_path,
                                                   path);
                    if (status < 0) {
                        error_count++;
                        logmsg(ERROR, BATON_CAT,
                               "Failed to resolve path '%s'", path);
                    }
                    else {
                        baton_error_t error;

                        // TODO - Make fetching metadata on results optional
                        json_t *avus =
                            list_metadata(conn, &rods_path, NULL, &error);

                        if (error.code != 0) error_count++;
                        if (avus) {
                            json_object_set_new(result, JSON_AVUS_KEY, avus);
                            print_json(result);
                        }
                    }

                    if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);
                }

                free(path);
            }

            json_decref(results);
        }

        json_decref(target);
    }

    rcDisconnect(conn);

    return 0;

error:
    if (conn) rcDisconnect(conn);

    return 1;
}
