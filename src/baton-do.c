/**
 * Copyright (C) 2017 Genome Research Ltd. All rights reserved.
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

static size_t default_buffer_size = 1024 * 64 * 16 * 2;

int main(int argc, char *argv[]) {
    option_flags flags = 0;
    int exit_status    = 0;
    char *zone_name = NULL;
    char *json_file = NULL;
    FILE *input     = NULL;

    while (1) {
        static struct option long_options[] = {
            // Flag options
            {"debug",       no_argument, &debug_flag,      1},
            {"help",        no_argument, &help_flag,       1},
            {"silent",      no_argument, &silent_flag,     1},
            {"unbuffered",  no_argument, &unbuffered_flag, 1},
            {"unsafe",      no_argument, &unsafe_flag,     1},
            {"verbose",     no_argument, &verbose_flag,    1},
            {"version",     no_argument, &version_flag,    1},
            // Indexed options
            {"file",        required_argument, NULL, 'f'},
            {"zone",        required_argument, NULL, 'z'},
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

    const char *help =
        "Name\n"
        "    baton-do\n"
        "\n"
        "Synopsis\n"
        "\n"
        "    baton-do [--file <JSON file>] [--silent]\n"
        "             [--unbuffered] [--verbose] [--version]\n"
        "\n"
        "Description\n"
        "    Performs remote operations as described in the JSON\n"
        "    input file.\n"
        ""
        "    --file        The JSON file describing the operations.\n"
        "                  Optional, defaults to STDIN.\n"
        "    --silent      Silence error messages.\n"
        "    --unbuffered  Flush print operations for each JSON object.\n"

        "    --verbose     Print verbose messages to STDERR.\n"
        "    --version     Print the version number and exit.\n"
        "    --zone        The zone to operate within. Optional.\n";

    if (help_flag) {
        printf("%s\n",help);
        exit(0);
    }

    if (version_flag) {
        printf("%s\n", VERSION);
        exit(0);
    }

    if (unbuffered_flag) flags = flags | FLUSH;
    if (unsafe_flag)     flags = flags | UNSAFE_RESOLVE;

    if (debug_flag)   set_log_threshold(DEBUG);
    if (verbose_flag) set_log_threshold(NOTICE);
    if (silent_flag)  set_log_threshold(FATAL);

    declare_client_name(argv[0]);
    input = maybe_stdin(json_file);

    operation_args_t args = { .flags       = flags,
                              .buffer_size = default_buffer_size,
                              .zone_name   = zone_name };

    int status = do_operation(input, baton_json_dispatch_op, &args);
    if (input != stdin) fclose(input);

    if (status != 0) exit_status = 5;

    exit(exit_status);
}
