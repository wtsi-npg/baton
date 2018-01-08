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

static int checksum_flag      = 0;
static int debug_flag         = 0;
static int help_flag          = 0;
static int silent_flag        = 0;
static int single_server_flag = 0;
static int unbuffered_flag    = 0;
static int unsafe_flag        = 0;
static int verbose_flag       = 0;
static int version_flag       = 0;

static size_t default_buffer_size = 1024 * 64 * 16 * 2;
static size_t max_buffer_size     = 1024 * 1024 * 1024;

int main(int argc, char *argv[]) {
    option_flags flags = 0;
    int exit_status    = 0;
    char *zone_name = NULL;
    char *json_file = NULL;
    FILE *input     = NULL;
    size_t buffer_size = default_buffer_size;

    while (1) {
        static struct option long_options[] = {
            // Flag options
            {"checksum",      no_argument, &checksum_flag,      1},
            {"debug",         no_argument, &debug_flag,         1},
            {"help",          no_argument, &help_flag,          1},
            {"silent",        no_argument, &silent_flag,        1},
            {"single-server", no_argument, &single_server_flag, 1},
            {"unbuffered",    no_argument, &unbuffered_flag,    1},
            {"unsafe",        no_argument, &unsafe_flag,        1},
            {"verbose",       no_argument, &verbose_flag,       1},
            {"version",       no_argument, &version_flag,       1},
            // Indexed options
            {"file",          required_argument, NULL, 'f'},
            {"buffer-size",   required_argument, NULL, 'b'},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        int c = getopt_long_only(argc, argv, "b:f:",
                                 long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1) break;

        switch (c) {
            case 'b':
                buffer_size = parse_size(optarg);
                if (errno != 0) buffer_size = default_buffer_size;
                break;

            case 'f':
                json_file = optarg;
                break;

            case '?':
                // getopt_long already printed an error message
                break;

            default:
                // Ignore
                break;
        }
    }

    if (checksum_flag)      flags = flags | CALCULATE_CHECKSUM;
    if (single_server_flag) flags = flags | SINGLE_SERVER;
    if (unsafe_flag)        flags = flags | UNSAFE_RESOLVE;
    if (unbuffered_flag)    flags = flags | FLUSH;

    const char *help =
        "Name\n"
        "    baton-put\n"
        "\n"
        "Synopsis\n"
        "\n"
        "    baton-put [--file <JSON file>] [--silent]\n"
        "              [--unbuffered] [--unsafe]\n"
        "              [--verbose] [--version]\n"
        "\n"
        "Description\n"
        "    Puts the contents of files into data objects described in a\n"
        "    JSON input file.\n"
        ""
        "    --buffer-size   Set the transfer buffer size.\n"
        "    --checksum      Calculate a checksum on the server side.\n"
        "    --file          The JSON file describing the data objects.\n"
        "                    Optional, defaults to STDIN.\n"
        "    --silent        Silence error messages.\n"
        "    --single-server Only connect to a single iRODS server\n"
        "    --unbuffered    Flush print operations for each JSON object.\n"
        "    --unsafe        Permit unsafe relative iRODS paths.\n"
        "    --verbose       Print verbose messages to STDERR.\n"
        "    --version       Print the version number and exit.\n";

    if (help_flag) {
        printf("%s\n",help);
        exit(0);
    }

    if (version_flag) {
        printf("%s\n", VERSION);
        exit(0);
    }

    if (debug_flag)   set_log_threshold(DEBUG);
    if (verbose_flag) set_log_threshold(NOTICE);
    if (silent_flag)  set_log_threshold(FATAL);

    declare_client_name(argv[0]);
    input = maybe_stdin(json_file);

    operation_args_t args = { .flags       = flags,
                              .buffer_size = default_buffer_size,
                              .zone_name   = zone_name };

    int status;
    if (flags & SINGLE_SERVER) {
        logmsg(DEBUG, "Single-server mode, falling back to operation 'write'");

        if (buffer_size > max_buffer_size) {
            logmsg(WARN, "Requested transfer buffer size %zu exceeds maximum of "
                   "%zu. Setting buffer size to %zu",
                   buffer_size, max_buffer_size, max_buffer_size);
            buffer_size = max_buffer_size;
        }

        if (buffer_size % 1024 != 0) {
            size_t tmp = ((buffer_size / 1024) + 1) * 1024;
            if (tmp > max_buffer_size) {
                tmp = max_buffer_size;
            }

            if (tmp > buffer_size) {
                buffer_size = tmp;
                logmsg(NOTICE, "Rounding transfer buffer size upwards from "
                       "%zu to %zu", buffer_size, tmp);
            }
        }

        logmsg(DEBUG, "Using a transfer buffer size of %zu bytes", buffer_size);
        status = do_operation(input, baton_json_write_op, &args);
    }
    else {
      status = do_operation(input, baton_json_put_op, &args);
    }

    if (input != stdin) fclose(input);

    if (status != 0)    exit_status = 5;

    exit(exit_status);
}
