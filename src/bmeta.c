#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "config.h"

#include "zlog.h"

#include "rodsClient.h"
#include "rodsPath.h"

#include "baton.h"

static char *LOG_CONF_FILE = ZLOG_CONF;

static int help_flag;
static int version_flag;

int main(int argc, char *argv[]) {

    int status;
    int exit_status;

    char *err_name;
    char *err_subname;
    char *path;

    metadata_op operation = -1;
    char *attr_name;
    char *attr_value;
    char *attr_unit = "";

    rodsEnv env;
    rcComm_t *conn;
    rodsPath_t rods_path;

    while (1) {
        static struct option long_options[] = {
            // Flag options
            {"help",    no_argument, &help_flag,    1},
            {"version", no_argument, &version_flag, 1},
            // Indexed options
            {"add",     no_argument,       NULL, 'd'},
            {"attr",    required_argument, NULL, 'a'},
            {"logconf", required_argument, NULL, 'l'},
            {"units",   required_argument, NULL, 'u'},
            {"value",   required_argument, NULL, 'v'},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        int c = getopt_long_only(argc, argv, "a:l:u:v:",
                                 long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c) {
            case 'd':
                operation = META_ADD;
                break;

            case 'a':
                attr_name = optarg;
                break;

            case 'u':
                attr_unit = optarg;
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
        puts("Print usage help here\n");
        exit(0);
    }

    if (version_flag) {
        printf("%s\n", VERSION);
        exit(0);
    }

    switch (operation) {
        case META_ADD:
            break;

        default:
            fprintf(stderr, "No operation was specified; valid operations "
                    "are: --add\n");
            goto args_error;
    }

    if (!attr_name) {
        fprintf(stderr, "An --attr argument is required\n");
        goto args_error;
    }

    if (!attr_value) {
        fprintf(stderr, "A --value argument is required\n");
        goto args_error;
    }

    if (zlog_init(LOG_CONF_FILE)) {
        fprintf(stderr, "Logging configuration failed\n");
    }

    if (optind >= argc) {
        fprintf(stderr, "Expected one or more iRODS paths as arguments\n");
        goto args_error;
    }
    else {
        conn = rods_login(&env);

        while (optind < argc) {
            path = argv[optind++];
            status = resolve_rods_path(conn, &env, &rods_path, path);

            if (status < 0) {
                exit_status = 5;
                logmsg(ERROR, BMETA_CAT, "Failed to resolve path '%s'", path);
                goto error;
            }

            status = modify_metadata(conn, &rods_path, operation,
                                     attr_name, attr_value, attr_unit);
            if (status < 0) {
                exit_status = 5;
                err_name = rodsErrorName(status, &err_subname);
                logmsg(ERROR, BMETA_CAT,
                       "Failed to add metadata ['%s' '%s' '%s'] to '%s': "
                       "error %d %s %s",
                       attr_name, attr_value, attr_unit, rods_path.outPath,
                       status, err_name, err_subname);
            }
        }

        rcDisconnect(conn);
    }

    zlog_fini();
    exit(exit_status);

args_error:
    exit_status = 4;

error:
    if (conn != NULL)
        rcDisconnect(conn);

    zlog_fini();
    exit(exit_status);
}
