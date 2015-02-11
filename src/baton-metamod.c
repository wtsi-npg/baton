/**
 * Copyright (c) 2013, 2014, 2015 Genome Research Ltd. All rights
 * reserved.
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

#include <jansson.h>

#include "config.h"

#ifdef HAVE_IRODS3
#include "rodsClient.h"
#include "rodsPath.h"
#endif

#ifdef HAVE_IRODS4
#include "rodsClient.hpp"
#include "rodsPath.hpp"
#endif

#include "baton.h"
#include "json.h"
#include "log.h"

static int debug_flag      = 0;
static int help_flag       = 0;
static int unbuffered_flag = 0;
static int verbose_flag    = 0;
static int version_flag    = 0;

int do_modify_metadata(FILE *input, metadata_op operation);

int main(int argc, char *argv[]) {
    int exit_status = 0;
    metadata_op meta_op = -1;
    int status;
    char *json_file = NULL;
    FILE     *input = NULL;

    while (1) {
        static struct option long_options[] = {
            // Flag options
            {"debug",      no_argument, &debug_flag,      1},
            {"help",       no_argument, &help_flag,       1},
            {"unbuffered", no_argument, &unbuffered_flag, 1},
            {"verbose",    no_argument, &verbose_flag,    1},
            {"version",    no_argument, &version_flag,    1},
            // Indexed options
            {"file",      required_argument, NULL, 'f'},
            {"operation", required_argument, NULL, 'o'},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        int c = getopt_long_only(argc, argv, "f:o:",
                                 long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1) break;

        switch (c) {
            case 'f':
                json_file = optarg;
                break;

            case 'o':
                if (strcmp("add", optarg) ==  0) {
                    meta_op = META_ADD;
                }
                if (strcmp("rem", optarg) == 0) {
                    meta_op = META_REM;
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
        puts("    baton-metamod");
        puts("");
        puts("Synopsis");
        puts("");
        puts("    baton-metamod [--file <JSON file>] --operation <operation>");
        puts("                  [--unbuffered] [--verbose] [--version]");
        puts("");
        puts("Description");
        puts("    Modifies metadata AVUs on collections and data objects");
        puts("described in a JSON input file.");
        puts("");
        puts("    --file        The JSON file describing the data objects and");
        puts("                  collections. Optional, defaults to STDIN.");
        puts("    --operation   Operation to perform. One of [add, rem].");
        puts("                  Required.");
        puts("    --unbuffered  Flush print operations for each JSON object.");
        puts("    --verbose     Print verbose messages to STDERR.");
        puts("    --version     Print the version number and exit.");
        puts("");

        exit(0);
    }

    if (version_flag) {
        printf("%s\n", VERSION);
        exit(0);
    }

    if (debug_flag)   set_log_threshold(DEBUG);
    if (verbose_flag) set_log_threshold(NOTICE);

    declare_client_name(argv[0]);

    switch (meta_op) {
        case META_ADD:
        case META_REM:
            input = maybe_stdin(json_file);
            status = do_modify_metadata(input, meta_op);
            if (status != 0) exit_status = 5;
            break;

        default:
            fprintf(stderr, "No valid operation was specified; valid "
                    "operations are: [add rem]\n");
            goto args_error;
    }

    exit(exit_status);

args_error:
    exit_status = 4;

    exit(exit_status);
}

int do_modify_metadata(FILE *input, metadata_op operation) {
    int path_count  = 0;
    int error_count = 0;

    rodsEnv env;
    rcComm_t *conn = rods_login(&env);
    if (!conn) goto error;

    while (!feof(input)) {
        size_t flags = JSON_DISABLE_EOF_CHECK | JSON_REJECT_DUPLICATES;
        json_error_t load_error;
        json_t *target = json_loadf(input, flags, &load_error);
        if (!target) {
            if (!feof(input)) {
                logmsg(ERROR, "JSON error at line %d, column %d: %s",
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
            json_t *avus = json_object_get(target, JSON_AVUS_KEY);
            if (!json_is_array(avus)) {
                error_count++;
                set_baton_error(&path_error, -1,
                                "AVU data for %s is not in a JSON array", path);
                add_error_value(target, &path_error);
            }
            else {
                rodsPath_t rods_path;
                int status = resolve_rods_path(conn, &env, &rods_path, path);
                if (status < 0) {
                    error_count++;
                    set_baton_error(&path_error, status,
                                    "Failed to resolve path '%s'", path);
                    add_error_value(target, &path_error);
                }
                else {
                    for (size_t i = 0; i < json_array_size(avus); i++) {
                        json_t *avu = json_array_get(avus, i);
                        baton_error_t mod_error;
                        modify_json_metadata(conn, &rods_path, operation, avu,
                                             &mod_error);

                        // FIXME: this only records the last error
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
        if (unbuffered_flag) fflush(stdout);

        json_decref(target);
        free(path);
    } // while

    rcDisconnect(conn);

    logmsg(DEBUG, "Processed %d paths with %d errors", path_count, error_count);

    return error_count;

error:
    if (conn) rcDisconnect(conn);

    logmsg(ERROR, "Processed %d paths with %d errors", path_count, error_count);

    return 1;
}
