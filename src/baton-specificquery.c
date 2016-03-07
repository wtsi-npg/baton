/**
 * Copyright (c) 2013, 2014, 2015, 2016 Genome Research Ltd. All
 * rights reserved.
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
 * @author Joshua C. Randall <jcrandall@alum.mit.edu>
 * @author Keith James <kdj@sanger.ac.uk>
 */

#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "rodsClient.h"
#include "rodsPath.h"

#include "baton.h"
#include "config.h"
#include "json.h"
#include "log.h"

static int debug_flag      = 0;
static int help_flag       = 0;
static int unbuffered_flag = 0;
static int verbose_flag    = 0;
static int version_flag    = 0;

int do_search_specific(FILE *input, char *zone_name);

int main(int argc, char *argv[]) {
    int exit_status = 0;
    char *zone_name = NULL;
    char *json_file = NULL;
    FILE *input     = NULL;

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
            {"zone",      required_argument, NULL, 'z'},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        int c = getopt_long_only(argc, argv, "f:z:", long_options,
                                 &option_index);

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


    if (help_flag) {
        puts("Name");
        puts("    baton-specificquery");
        puts("");
        puts("Synopsis");
        puts("");
        puts("    baton-specificquery");
        puts("                    [--file <JSON file>]");
        puts("                    [--unbuffered] [--verbose] [--version]");
        puts("                    [--zone <name>]");
        puts("");
        puts("Description");
        puts("    Runs a specific SQL query (must have been installed by");
        puts("`iadmin asq`) specified in a JSON input file.");
        puts("");
        puts("    --file        The JSON file describing the query. Optional,");
        puts("                  defaults to STDIN.");
        puts("    --unbuffered  Flush print operations for each JSON object.");
        puts("    --verbose     Print verbose messages to STDERR.");
        puts("    --version     Print the version number and exit.");
        puts("    --zone        The zone to search. Optional.\n");
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
    int status = do_search_specific(input, zone_name);
    if (status != 0) exit_status = 5;

    exit(exit_status);
}

int do_search_specific(FILE *input, char *zone_name) {
    int item_count  = 0;
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

        item_count++;
        if (!json_is_object(target)) {
            logmsg(ERROR, "Item %d in stream was not a JSON object; skipping",
                   item_count);
            error_count++;
            json_decref(target);
            continue;
        }

        json_t *results = NULL;

        baton_error_t search_error;
        results = search_specific(conn, target, zone_name, &search_error);
        if (search_error.code != 0) {
            error_count++;
            add_error_value(target, &search_error);
            print_json(target);
        }
        else {
            print_json(results);
        }

        if (unbuffered_flag) fflush(stdout);

        if (results) json_decref(results);
        if (target) json_decref(target);
    } // while

    rcDisconnect(conn);

    logmsg(DEBUG, "Processed %d items with %d errors", item_count, error_count);

    return 0;

error:
    if (conn) rcDisconnect(conn);

    logmsg(ERROR, "Processed %d items with %d errors", item_count, error_count);

    return 1;
}
