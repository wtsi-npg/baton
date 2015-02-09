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

#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

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

static int acl_flag        = 0;
static int avu_flag        = 0;
static int coll_flag       = 0;
static int debug_flag      = 0;
static int help_flag       = 0;
static int obj_flag        = 0;
static int size_flag       = 0;
static int timestamp_flag  = 0;
static int unbuffered_flag = 0;
static int verbose_flag    = 0;
static int version_flag    = 0;

int do_search_metadata(FILE *input, char *zone_name, option_flags oflags);

int main(int argc, char *argv[]) {
    option_flags oflags = SEARCH_COLLECTIONS | SEARCH_OBJECTS;
    int exit_status = 0;
    char *zone_name = NULL;
    char *json_file = NULL;
    FILE *input     = NULL;

    while (1) {
        static struct option long_options[] = {
            // Flag options
            {"acl",        no_argument, &acl_flag,        1},
            {"avu",        no_argument, &avu_flag,        1},
            {"coll",       no_argument, &coll_flag,       1},
            {"debug",      no_argument, &debug_flag,      1},
            {"help",       no_argument, &help_flag,       1},
            {"obj",        no_argument, &obj_flag,        1},
            {"size",       no_argument, &size_flag,       1},
            {"timestamp",  no_argument, &timestamp_flag,  1},
            {"unbuffered", no_argument, &unbuffered_flag, 1},
            {"verbose",    no_argument, &verbose_flag,    1},
            {"version",    no_argument, &version_flag,    1},
            // Indexed options
            {"file",      required_argument, NULL, 'f'},
            {"zone",      required_argument, NULL, 'z'},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        int c = getopt_long_only(argc, argv, "f:z:",
                                 long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1) break;

        switch (c) {
            case 'f':
                json_file = optarg;
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

    if (coll_flag && !obj_flag)  {
        oflags = oflags ^ SEARCH_OBJECTS;
    }
    else if (obj_flag && !coll_flag)  {
        oflags = oflags ^ SEARCH_COLLECTIONS;
    }

    if (acl_flag)       oflags = oflags | PRINT_ACL;
    if (avu_flag)       oflags = oflags | PRINT_AVU;
    if (size_flag)      oflags = oflags | PRINT_SIZE;
    if (timestamp_flag) oflags = oflags | PRINT_TIMESTAMP;

    if (help_flag) {
        puts("Name");
        puts("    baton-metaquery");
        puts("");
        puts("Synopsis");
        puts("");
        puts("    baton-metaquery [--acl] [--avu] [--coll]");
        puts("                    [--file <JSON file>] [--obj ] [--size]");
        puts("                    [--timestamp] [--unbuffered] [--verbose]");
        puts("                    [--version] [--zone <name>]");
        puts("");
        puts("Description");
        puts("    Finds items in iRODS by AVU, given a query constructed");
        puts("from a JSON input file.");
        puts("");
        puts("    --acl         Print access control lists in output.");
        puts("    --avu         Print AVU lists in output.");
        puts("    --coll        Limit search to collection metadata only.");
        puts("    --file        The JSON file describing the query. Optional,");
        puts("                  defaults to STDIN.");
        puts("    --obj         Limit search to data object metadata only.");
        puts("    --timestamp   Print timestamps in output.");
        puts("    --unbuffered  Flush print operations for each JSON object.");
        puts("    --verbose     Print verbose messages to STDERR.");
        puts("    --version     Print the version number and exit.");
        puts("    --zone        The zone to search. Optional.");
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
    input = maybe_stdin(json_file);
    int status = do_search_metadata(input, zone_name, oflags);
    if (status != 0) exit_status = 5;

    exit(exit_status);
}

int do_search_metadata(FILE *input, char *zone_name, option_flags oflags) {
    int error_count = 0;

    rodsEnv env;
    rcComm_t *conn = rods_login(&env);
    if (!conn) goto error;

    while (!feof(input)) {
        size_t jflags = JSON_DISABLE_EOF_CHECK | JSON_REJECT_DUPLICATES;
        json_error_t load_error;
        json_t *target = json_loadf(input, jflags, &load_error);
        if (!target) {
            if (!feof(input)) {
                logmsg(ERROR, "JSON error at line %d, column %d: %s",
                       load_error.line, load_error.column, load_error.text);
            }

            continue;
        }

        baton_error_t error;
        json_t *results = search_metadata(conn, target, zone_name, oflags,
                                          &error);
        if (error.code != 0) {
            error_count++;
            add_error_value(target, &error);
            print_json(target);
        }
        else {
            print_json(results);
            json_decref(results);
        }

        if (unbuffered_flag) fflush(stdout);
        json_decref(target);
    } // while

    rcDisconnect(conn);

    return 0;

error:
    if (conn) rcDisconnect(conn);

    return 1;
}
