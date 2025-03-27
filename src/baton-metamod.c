/**
 * Copyright (C) 2013, 2014, 2015, 2017, 2019, 2021, 2025 Genome Research
 * Ltd. All rights reserved.
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
#include "baton.h"

static int debug_flag      = 0;
static int help_flag       = 0;
static int silent_flag     = 0;
static int unbuffered_flag = 0;
static int unsafe_flag     = 0;
static int verbose_flag    = 0;
static int version_flag    = 0;

int main(const int argc, char *argv[]) {
    option_flags flags = 0;
    int exit_status = 0;
    const char *json_file = NULL;
    FILE     *input = NULL;
    unsigned long max_connect_time = DEFAULT_MAX_CONNECT_TIME;

    while (1) {
        static struct option long_options[] = {
            // Flag options
            {"debug",      no_argument, &debug_flag,      1},
            {"help",       no_argument, &help_flag,       1},
            {"silent",     no_argument, &silent_flag,     1},
            {"unbuffered", no_argument, &unbuffered_flag, 1},
            {"unsafe",     no_argument, &unsafe_flag,     1},
            {"verbose",    no_argument, &verbose_flag,    1},
            {"version",    no_argument, &version_flag,    1},
            // Indexed options
            {"connect-time", required_argument, NULL, 'c'},
            {"file",         required_argument, NULL, 'f'},
            {"operation",    required_argument, NULL, 'o'},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        const int c = getopt_long_only(argc, argv, "c:f:o:",
                                       long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1) break;

        switch (c) {
            case 'c':
                errno = 0;
                char *end_ptr;
                const unsigned long val = strtoul(optarg, &end_ptr, 10);

                if ((errno == ERANGE && val == ULONG_MAX) ||
                    (errno != 0 && val == 0)              ||
                    end_ptr == optarg) {
                    fprintf(stderr, "Invalid --connect-time '%s'\n", optarg);
                    exit(1);
                }

                max_connect_time = val;
                break;

            case 'f':
                json_file = optarg;
                break;

            case 'o':
                if (strcmp("add", optarg) ==  0) {
                    flags = flags | ADD_AVU;
                }
                else if (strcmp("rem", optarg) == 0) {
                    flags = flags | REMOVE_AVU;
                }
                else {
                    fprintf(stderr, "No valid operation was specified; valid "
                            "operations are: [add rem]\n");
                    goto args_error;
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

    const char *help =
        "Name\n"
        "    baton-metamod\n"
        "\n"
        "Synopsis\n"
        "\n"
        "    baton-metamod [--connect-time <n>] [--file <JSON file>]\n"
        "                  --operation <operation>\n"
        "                  [--silent] [--unbuffered] [--unsafe]\n"
        "                  [--verbose] [--version]\n"
        "\n"
        "Description\n"
        "    Modifies metadata AVUs on collections and data objects\n"
        "described in a JSON input file.\n"
        "\n"
        "  --connect-time  The duration in seconds after which a connection\n"
        "                  to iRODS will be refreshed (closed and reopened\n"
        "                  between JSON documents) to allow iRODS server\n"
        "                  resources to be released. Optional, defaults to\n"
        "                  10 minutes.\n"
        "    --file        The JSON file describing the data objects and\n"
        "                  collections. Optional, defaults to STDIN.\n"
        "    --operation   Operation to perform. One of [add, rem].\n"
        "                  Required.\n"
        "    --silent      Silence error messages.\n"
        "    --unbuffered  Flush print operations for each JSON object.\n"
        "    --unsafe      Permit unsafe relative iRODS paths.\n"
        "    --verbose     Print verbose messages to STDERR.\n"
        "    --version     Print the version number and exit.\n";

    if (help_flag) {
        printf("%s\n", help);
        exit(0);
    }

    if (version_flag) {
        printf("%s\n", VERSION);
        exit(0);
    }

    if (unsafe_flag)     flags = flags | UNSAFE_RESOLVE;
    if (unbuffered_flag) flags = flags | FLUSH;

    if (debug_flag)   set_log_threshold(DEBUG);
    if (verbose_flag) set_log_threshold(NOTICE);
    if (silent_flag)  set_log_threshold(FATAL);

    declare_client_name(argv[0]);
    input = maybe_stdin(json_file);
    if (!input) {
        exit(1);
    }

    operation_args_t args = { .flags            = flags,
                              .max_connect_time = max_connect_time };

    const int status = do_operation(input, baton_json_metamod_op, &args);
    if (input != stdin) fclose(input);

    if (status != 0) exit_status = 5;

    exit(exit_status);

args_error:
    exit_status = 4;

    exit(exit_status);
}
