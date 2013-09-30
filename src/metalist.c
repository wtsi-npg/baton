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

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "rodsClient.h"
#include "rodsPath.h"
#include <zlog.h>

#include "baton.h"
#include "config.h"
#include "json.h"
#include "utilities.h"

static char *SYSTEM_LOG_CONF_FILE = ZLOG_CONF;

static char *USER_LOG_CONF_FILE = NULL;

static int help_flag;
static int version_flag;

int do_list_metadata(int argc, char *argv[], int optind, char *attr_name);

int main(int argc, char *argv[]) {
    int exit_status;
    char *attr_name = NULL;

    while (1) {
        static struct option long_options[] = {
            // Flag options
            {"help",      no_argument, &help_flag,    1},
            {"version",   no_argument, &version_flag, 1},
            // Indexed options
            {"attr",      required_argument, NULL, 'a'},
            {"logconf",   required_argument, NULL, 'l'},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        int c = getopt_long_only(argc, argv, "a:l:",
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
        puts("    metalist");
        puts("");
        puts("Synopsis");
        puts("");
        puts("    metalist [--attr <attr>] <paths ...>");
        puts("");
        puts("Description");
        puts("    Lists metadata AVUs.");
        puts("");
        puts("    --attr        The attribute of the AVU. Optional.");
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
    else {
        if (zlog_init(USER_LOG_CONF_FILE)) {
            fprintf(stderr, "Logging configuration failed "
                    "(using user-defined configuration in '%s')\n",
                    USER_LOG_CONF_FILE);
        }
    }

    int status = do_list_metadata(argc, argv, optind, attr_name);
    if (status != 0) exit_status = 5;

    zlog_fini();
    exit(exit_status);

args_error:
    exit_status = 4;

error:
    zlog_fini();
    exit(exit_status);
}

int do_list_metadata(int argc, char *argv[], int optind, char *attr_name) {
    int path_count = 0;
    int error_count = 0;

    rodsEnv env;
    rcComm_t *conn = rods_login(&env);
    if (!conn) goto error;

    while (optind < argc) {
        char *path = argv[optind++];
        rodsPath_t rods_path;
        path_count++;

        int status = resolve_rods_path(conn, &env, &rods_path, path);
        if (status < 0) {
            error_count++;
            logmsg(ERROR, BATON_CAT, "Failed to resolve path '%s'", path);
        }
        else {
            struct baton_error error;
            json_t *results =
                list_metadata(conn, &rods_path, attr_name, &error);

            if (error.code != 0) error_count++;

            if (results) {
                print_json(results);
                json_decref(results);
            }
        }
    }

    rcDisconnect(conn);

    logmsg(DEBUG, BATON_CAT, "Processed %d paths with %d errors",
           path_count, error_count);

    return error_count;

error:
    if (conn) rcDisconnect(conn);

    logmsg(ERROR, BATON_CAT, "Processed %d paths with %d errors",
           path_count, error_count);

    return -1;
}
