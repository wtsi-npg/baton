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

#include "config.h"
#include "zlog.h"
#include "rodsClient.h"
#include "rodsPath.h"
#include "baton.h"

static char *SYSTEM_LOG_CONF_FILE = ZLOG_CONF;

static char *USER_LOG_CONF_FILE = NULL;

static int help_flag;
static int version_flag;

int do_modify_metadata(int argc, char *argv[], int optind,
                       metadata_op operation,
                       char *attr_name, char *attr_value, char *attr_unit);

int main(int argc, char *argv[]) {

    int exit_status;

    metadata_op meta_op = -1;

    char *attr_name = NULL;
    char *attr_value = NULL;
    char *attr_units = "";

    while (1) {
        static struct option long_options[] = {
            // Flag options
            {"help",      no_argument, &help_flag,    1},
            {"version",   no_argument, &version_flag, 1},
            // Indexed options
            {"attr",      required_argument, NULL, 'a'},
            {"logconf",   required_argument, NULL, 'l'},
            {"operation", required_argument, NULL, 'o'},
            {"units",     required_argument, NULL, 'u'},
            {"value",     required_argument, NULL, 'v'},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        int c = getopt_long_only(argc, argv, "a:l:o:u:v:",
                                 long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c) {
            case 'a':
                attr_name = optarg;
                break;

            case 'l':
                USER_LOG_CONF_FILE = optarg;
                break;

            case 'o':
                if (strcmp("add", optarg) ==  0) {
                    meta_op = META_ADD;
                }

                break;

            case 'u':
                attr_units = optarg;
                break;

            case 'v':
                attr_value = optarg;
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
        puts("    metamod");
        puts("");
        puts("Synopsis");
        puts("");
        puts("    metamod -o <operation> --attr <attr> --value <value> [--units <unit>] \\");
        puts("      <paths ...>");
        puts("");
        puts("Description");
        puts("    Modifies metadata AVUs.");
        puts("");
        puts("    --operation   Operation to perform. One of [add]. Required.");
        puts("    --attr        The attribute of the AVU. Required.");
        puts("    --value       The value of the AVU. Required");
        puts("    --units       The units of the AVU. Optional");
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
            if (!attr_name) {
                fprintf(stderr, "An --attr argument is required\n");
                goto args_error;
            }

            if (!attr_value) {
                fprintf(stderr, "A --value argument is required\n");
                goto args_error;
            }

            if (optind >= argc) {
                fprintf(stderr,
                        "Expected one or more iRODS paths as arguments\n");
                goto args_error;
            }

            int status = do_modify_metadata(argc, argv, optind, meta_op,
                                            attr_name, attr_value, attr_units);
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
                       metadata_op operation,
                       char *attr_name, char *attr_value, char *attr_units) {
    char *err_name;
    char *err_subname;

    int path_count = 0;
    int error_count = 0;

    rodsEnv env;
    rodsPath_t rods_path;
    rcComm_t *conn = rods_login(&env);
    if (!conn) {
        goto error;
    }

    while (optind < argc) {
        char *path = argv[optind++];
        int status;
        path_count++;

        status = resolve_rods_path(conn, &env, &rods_path, path);
        if (status < 0) {
            error_count++;
            logmsg(ERROR, BATON_CAT, "Failed to resolve path '%s'", path);
        }
        else {
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

    rcDisconnect(conn);

    logmsg(DEBUG, BATON_CAT, "Processed %d paths with %d errors",
           path_count, error_count);

    return error_count;

error:
    if (conn) {
        rcDisconnect(conn);
    }

    logmsg(ERROR, BATON_CAT, "Processed %d paths with %d errors",
           path_count, error_count);

    return 1;
}
