/**
 * Copyright (c) 2014 Genome Research Ltd. All rights reserved.
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

#include "rodsClient.h"
#include "rodsPath.h"
#include <jansson.h>

#include "baton.h"
#include "config.h"
#include "json.h"
#include "log.h"
#include "read.h"

static int acl_flag        = 0;
static int avu_flag        = 0;
static int debug_flag      = 0;
static int help_flag       = 0;
static int raw_flag        = 0;
static int save_flag       = 0;
static int size_flag       = 0;
static int timestamp_flag  = 0;
static int unbuffered_flag = 0;
static int verbose_flag    = 0;
static int version_flag    = 0;

static size_t default_buffer_size = 1024 * 64 * 16 * 2;
static size_t max_buffer_size     = 1024 * 1024 * 1024;

int do_get_files(FILE *input, option_flags oflags, size_t buffer_size);
int write_to_stream(rcComm_t *conn, rodsPath_t *rods_path, FILE *out,
                    baton_error_t *error);

int main(int argc, char *argv[]) {
    option_flags oflags = 0;
    int exit_status = 0;
    char *json_file = NULL;
    FILE *input     = NULL;
    size_t buffer_size = default_buffer_size;

    while (1) {
        static struct option long_options[] = {
            // Flag options
            {"acl",         no_argument, &acl_flag,        1},
            {"avu",         no_argument, &avu_flag,        1},
            {"debug",       no_argument, &debug_flag,      1},
            {"help",        no_argument, &help_flag,       1},
            {"raw",         no_argument, &raw_flag,        1},
            {"save",        no_argument, &save_flag,       1},
            {"size",        no_argument, &size_flag,       1},
            {"timestamp",   no_argument, &timestamp_flag,  1},
            {"unbuffered",  no_argument, &unbuffered_flag, 1},
            {"verbose",     no_argument, &verbose_flag,    1},
            {"version",     no_argument, &version_flag,    1},
            // Indexed options
            {"file",        required_argument, NULL, 'f'},
            {"buffer-size", required_argument, NULL, 'b'},
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

    if (acl_flag)       oflags = oflags | PRINT_ACL;
    if (avu_flag)       oflags = oflags | PRINT_AVU;
    if (raw_flag)       oflags = oflags | PRINT_RAW;
    if (size_flag)      oflags = oflags | PRINT_SIZE;
    if (timestamp_flag) oflags = oflags | PRINT_TIMESTAMP;

    if (help_flag) {
        puts("Name");
        puts("    baton-get");
        puts("");
        puts("Synopsis");
        puts("");
        puts("    baton-get [--acl] [--avu] [--file <JSON file>]");
        puts("              [--raw] [--save] [--size] [--timestamp]");
        puts("              [--unbuffered] [--verbose] [--version]");
        puts("");
        puts("Description");
        puts("    Gets the contents of data objects described in a JSON");
        puts("    input file.");
        puts("");
        puts("    --acl         Print access control lists in output.");
        puts("    --avu         Print AVU lists in output.");
        puts("    --buffer-size Set the transfer buffer size.");
        puts("    --file        The JSON file describing the data objects.");
        puts("                  Optional, defaults to STDIN.");
        puts("    --raw         Print data object content without any JSON");
        puts("                  wrapping.");
        puts("    --save        Save data object content to individual files,");
        puts("                  without any JSON wrapping i.e. implies --raw.");
        puts("    --size        Print data object sizes in output.");
        puts("    --timestamp   Print timestamps in output.");
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
    if (raw_flag || save_flag) {
        const char *msg = "Ignoring the %s flag because raw output requested";

        if (acl_flag)       logmsg(WARN, msg, "--acl");
        if (avu_flag)       logmsg(WARN, msg, "--avu");
        if (size_flag)      logmsg(WARN, msg, "--size");
        if (timestamp_flag) logmsg(WARN, msg, "--timestamp");
    }

    declare_client_name(argv[0]);
    input = maybe_stdin(json_file);

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

    logmsg(DEBUG, "Using a transfer buffer size of %zu", buffer_size);

    int status = do_get_files(input, oflags, buffer_size);
    if (input != stdin) fclose(input);

    if (status != 0) exit_status = 5;

    exit(exit_status);
}

int do_get_files(FILE *input, option_flags oflags, size_t buffer_size) {
    int path_count  = 0;
    int error_count = 0;

    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    if (!conn) goto error;

    FILE *report_stream;
    if (raw_flag) {
      report_stream = stderr;
    }
    else {
      report_stream = stdout;
    }

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

        baton_error_t path_error;
        char *path = json_to_path(target, &path_error);
        path_count++;

        if (add_error_report(target, &path_error)) {
            error_count++;
            print_json_stream(target, report_stream);
        }
        else {
            rodsPath_t rods_path;
            int status = resolve_rods_path(conn, &env, &rods_path, path);
            if (status < 0) {
                set_baton_error(&path_error, status,
                                "Failed to resolve path '%s'", path);
                add_error_report(target, &path_error);
                error_count++;
                print_json_stream(target, report_stream);
            }
            else {
                baton_error_t error;

                if (save_flag) {
                    char *file = NULL;
                    file = json_to_local_path(target, &error);
                    if (file) {
                        write_path_to_file(conn, &rods_path, file,
                                           buffer_size, &error);
                        free(file);
                    }

                    if (add_error_report(target, &error)) {
                        error_count++;
                        print_json_stream(target, report_stream);
                    }
                }
                else if (raw_flag) {
                    write_path_to_stream(conn, &rods_path, stdout,
                                         buffer_size, &error);

                    if (add_error_report(target, &error)) {
                        error_count++;
                        print_json_stream(target, report_stream);
                    }
                }
                else {
                    json_t *results =
                        ingest_path(conn, &rods_path, oflags,
                                    buffer_size, &error);

                    if (add_error_report(target, &error)) {
                        error_count++;
                        print_json_stream(target, report_stream);
                    }
                    else {
                        print_json_stream(results, report_stream);
                        json_decref(results);
                    }
                }
            }

            if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);
        }

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
