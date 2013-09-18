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

#include <assert.h>
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

int query_metadata(char *attr_name, char *attr_value);

int obj_search_and_print(rcComm_t *conn, char *attr_name, char *attr_value);

int col_search_and_print(rcComm_t *conn, char *attr_name, char *attr_value);

genQueryInp_t *prepare_obj_search(genQueryInp_t *query_input, char *attr_name,
                                 char *attr_value);

genQueryInp_t *prepare_col_search(genQueryInp_t *query_input, char *attr_name,
                                  char *attr_value);


int main(int argc, char *argv[]) {
    int exit_status;

    char *attr_name = NULL;
    char *attr_value = NULL;

    while (1) {
        static struct option long_options[] = {
            // Flag options
            {"help",      no_argument, &help_flag,    1},
            {"version",   no_argument, &version_flag, 1},
            // Indexed options
            {"attr",      required_argument, NULL, 'a'},
            {"logconf",   required_argument, NULL, 'l'},
            {"value",     required_argument, NULL, 'v'},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        int c = getopt_long_only(argc, argv, "a:l:u:v:",
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
        puts("    metaquery");
        puts("");
        puts("Synopsis");
        puts("");
        puts("    metaquery --attr <attr> --value <value>");
        puts("");
        puts("Description");
        puts("    Finds items in iRODS by AVU.");
        puts("");
        puts("    --attr        The attribute of the AVU. Required.");
        puts("    --value       The value of the AVU. Required");
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


    if (!attr_name) {
        fprintf(stderr, "An --attr argument is required\n");
        goto args_error;
    }

    if (!attr_value) {
        fprintf(stderr, "A --value argument is required\n");
        goto args_error;
    }

    exit_status = query_metadata(attr_name, attr_value);

    zlog_fini();
    exit(exit_status);

args_error:
    exit_status = 4;

error:
    zlog_fini();
    exit(exit_status);
}

int query_metadata(char *attr_name, char *attr_value) {
    char *err_name;
    char *err_subname;

    int path_count = 0;
    int error_count = 0;

    rodsEnv env;
    rcComm_t *conn = rods_login(&env);
    if (conn == NULL) {
        goto error;
    }

    // TODO: bundle the results into one JSON collection
    obj_search_and_print(conn, attr_name, attr_value);
    col_search_and_print(conn, attr_name, attr_value);

    rcDisconnect(conn);

    return 0;

error:
    if (conn != NULL) {
        rcDisconnect(conn);
    }

    return -1;
}

int obj_search_and_print(rcComm_t *conn, char *attr_name, char *attr_value) {
    int num_columns = 2;
    int columns[] = { COL_COLL_NAME, COL_DATA_NAME };
    const char *labels[] = { "collection", "data_object" };

    // TODO: Improve error handling
    int max_rows = 10;
    genQueryInp_t *query_input = make_query_input(max_rows, num_columns,
                                                  columns);
    query_input = prepare_obj_search(query_input, attr_name, attr_value);

    int status = query_and_print(conn, query_input, labels);
    free_query_input(query_input);

    return status;
}

int col_search_and_print(rcComm_t *conn, char *attr_name, char *attr_value) {
    int num_columns = 1;
    int columns[] = { COL_COLL_NAME };
    const char *labels[] = { "collection" };

    // TODO: Improve error handling
    int max_rows = 10;
    genQueryInp_t *query_input = make_query_input(max_rows, num_columns,
                                                  columns);
    prepare_col_search(query_input, attr_name, attr_value);

    int status = query_and_print(conn, query_input, labels);
    free_query_input(query_input);

    return status;
}

genQueryInp_t *prepare_obj_search(genQueryInp_t *query_input, char *attr_name,
                                  char *attr_value) {
    query_cond an = { .column = COL_META_DATA_ATTR_NAME,
                      .operator = "=",
                      .value = attr_name };
    query_cond av = { .column = COL_META_DATA_ATTR_VALUE,
                      .operator = "=",
                      .value = attr_value };
    int num_conds = 2;
    return add_query_conds(query_input, num_conds, (query_cond []) { an, av });
}

genQueryInp_t *prepare_col_search(genQueryInp_t *query_input, char *attr_name,
                                  char *attr_value) {
    query_cond an = { .column = COL_META_COLL_ATTR_NAME,
                      .operator = "=",
                      .value = attr_name };
    query_cond av = { .column = COL_META_COLL_ATTR_VALUE,
                      .operator = "=",
                      .value = attr_value };
    int num_conds = 2;
    return add_query_conds(query_input, num_conds, (query_cond []) { an, av });
}
