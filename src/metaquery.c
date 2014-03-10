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

static char *SYSTEM_LOG_CONF_FILE = ZLOG_CONF; // Set by autoconf
static char *USER_LOG_CONF_FILE = NULL;

static int help_flag;
static int version_flag;

int do_search_metadata(char *attr_name, char *attr_value, char *root_path,
                       char *zone_name);

int main(int argc, char *argv[]) {
    int exit_status = 0;
    char *zone_name = NULL;
    char *attr_name = NULL;
    char *attr_value = NULL;
    char *root_path = NULL;

    while (1) {
        static struct option long_options[] = {
            // Flag options
            {"help",      no_argument, &help_flag,    1},
            {"version",   no_argument, &version_flag, 1},
            // Indexed options
            {"attr",      required_argument, NULL, 'a'},
            {"logconf",   required_argument, NULL, 'l'},
            {"root",      required_argument, NULL, 'r'},
            {"value",     required_argument, NULL, 'v'},
            {"zone",      required_argument, NULL, 'z'},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        int c = getopt_long_only(argc, argv, "a:l:r:v:z:",
                                 long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1) break;

        switch (c) {
            case 'a':
                attr_name = optarg;
                break;

            case 'l':
                USER_LOG_CONF_FILE = optarg;
                break;

            case 'v':
                attr_value = optarg;
                break;

            case 'r':
                root_path = optarg;
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
        puts("    metaquery");
        puts("");
        puts("Synopsis");
        puts("");
        puts("    metaquery --attr <attr> --value <value> [--root <path>]");
        puts("      [--zone <name>]");
        puts("");
        puts("Description");
        puts("    Finds items in iRODS by AVU.");
        puts("");
        puts("    --attr        The attribute of the AVU. Required.");
        puts("    --value       The value of the AVU. Required");
        puts("    --root        The root path under which to search. Optional");
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

    if (!attr_name) {
        fprintf(stderr, "An --attr argument is required\n");
        goto args_error;
    }

    if (!attr_value) {
        fprintf(stderr, "A --value argument is required\n");
        goto args_error;
    }

    exit_status = do_search_metadata(attr_name, attr_value, root_path,
                                     zone_name);

    zlog_fini();
    exit(exit_status);

args_error:
    exit_status = 4;

    zlog_fini();
    exit(exit_status);
}

int do_search_metadata(char *attr_name, char *attr_value, char *root_path,
                       char *zone_name) {
    int error_count = 0;

    rodsEnv env;
    rcComm_t *conn = rods_login(&env);
    if (!conn) goto error;

    json_t *target = query_args_to_json(attr_name, attr_value, root_path);

    baton_error_t error;
    json_t *results = search_metadata(conn, target, zone_name, PRINT_DEFAULT,
                                      &error);
    if (error.code != 0) {
        logmsg(ERROR, BATON_CAT, "Failed to search: %s", error.message);
        goto error;
    }
    else {
        for (size_t i = 0; i < json_array_size(results); i++) {
            json_t *result = json_array_get(results, i);

            baton_error_t path_error;
            char *path = json_to_path(result, &path_error);

            // This should always succeed because we are making the JSON
            if (path_error.code != 0) {
                error_count++;
            }
            else {
                printf("%s\n", path);
                free(path);
            }
        }
        json_decref(results);
    }

    rcDisconnect(conn);

    return 0;

error:
    if (conn) rcDisconnect(conn);

    return 1;
}
