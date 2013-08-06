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
 * @file baton.c
 */

#include "zlog.h"
#include <jansson.h>

#include "rodsType.h"
#include "rodsErrorTable.h"
#include "rodsClient.h"
#include "miscUtil.h"

#include "baton.h"

void logmsg(log_level level, const char* category, const char *format, ...) {
    va_list args;
    va_start(args, format);

    zlog_category_t *cat = zlog_get_category(category);
    if (cat == NULL) {
        fprintf(stderr, "Failed to get zlog category '%s'", category);
        goto error;
    }

    switch (level) {
        case FATAL:
            vzlog_fatal(cat, format, args);
            break;

        case ERROR:
            vzlog_error(cat, format, args);
            break;

        case WARN:
            vzlog_warn(cat, format, args);
            break;

        case NOTICE:
            vzlog_notice(cat, format, args);
            break;

        case INFO:
            vzlog_info(cat, format, args);
            break;

        case DEBUG:
            vzlog_debug(cat, format, args);
            break;

        default:
            vzlog_debug(cat, format, args);
    }

    va_end(args);
    return;

error:
    va_end(args);
    return;
}

void log_rods_errstack(log_level level, const char* category, rError_t *err) {
    rErrMsg_t *errmsg;

    int len = err->len;
    for (int i = 0; i < len; i++) {
	    errmsg = err->errMsg[i];
        logmsg(level, category, "Level %d: %s", i, errmsg->msg);
    }
}

rcComm_t *rods_login(rodsEnv *env) {
    int status;
    rErrMsg_t errmsg;

    status = getRodsEnv(env);
    if (status < 0) {
        logmsg(ERROR, BATON_CAT, "Failed to load your iRODS environment");
        goto error;
    }

    rcComm_t *conn = rcConnect(env->rodsHost, env->rodsPort, env->rodsUserName,
                               env->rodsZone, RECONN_TIMEOUT, &errmsg);
    if (conn == NULL) {
        logmsg(ERROR, BATON_CAT, "Failed to connect to %s:%d zone '%s' as '%s'",
               env->rodsHost, env->rodsPort, env->rodsZone, env->rodsUserName);
        goto error;
    }

    status = clientLogin(conn);
    if (status < 0) {
        logmsg(ERROR, BATON_CAT, "Failed to log in to iRODS");
        goto error;
    }

    return conn;

error:
    if (conn != NULL)
        rcDisconnect(conn);

    return NULL;
}


int init_rods_path(rodsPath_t *rodspath, char *inpath) {
    if (rodspath == NULL) {
        return USER__NULL_INPUT_ERR;
    }

    memset(rodspath, 0, sizeof(rodsPath_t));
    char *dest = rstrcpy(rodspath->inPath, inpath, MAX_NAME_LEN);
    if (dest == NULL) {
        return -1;
    }

    return 0;
}

int resolve_rods_path(rcComm_t *conn, rodsEnv *env,
                      rodsPath_t *rods_path, char *inpath) {
    int status;

    status = init_rods_path(rods_path, inpath);
    if (status < 0) {
        logmsg(ERROR, BATON_CAT, "Failed to create iRODS path '%s'", inpath);
        goto error;
    }

    status = parseRodsPath(rods_path, env);
    if (status < 0) {
        logmsg(ERROR, BATON_CAT, "Failed to parse path '%s'",
               rods_path->inPath);
        goto error;
    }

    status = getRodsObjType(conn, rods_path);
    if (status < 0) {
        logmsg(ERROR, BATON_CAT, "Failed to stat iRODS path '%s'",
               rods_path->inPath);
        goto error;
    }

    return status;

error:
    return status;
}

void map_mod_args(modAVUMetadataInp_t *out, mod_metadata_in *in) {
    out->arg0 = metadata_ops[in->op];
    out->arg1 = in->type_arg;
    out->arg2 = in->rods_path->outPath;
    out->arg3 = in->attr_name;
    out->arg4 = in->attr_value;
    out->arg5 = in->attr_units;
    out->arg6 = "";
    out->arg7 = "";
    out->arg8 = "";
    out->arg9 = "";

    return;
}

int modify_metadata(rcComm_t *conn, rodsPath_t *rods_path, metadata_op op,
                    char *attr_name, char *attr_value, char *attr_units) {
    int status;
    char *err_name;
    char *err_subname;
    char *type_arg;
    switch (rods_path->objType) {
        case DATA_OBJ_T:
            logmsg(DEBUG, BATON_CAT, "Indentified '%s' as a data object",
                   rods_path->outPath);
            type_arg = "-d";
            break;

        case COLL_OBJ_T:
            logmsg(DEBUG, BATON_CAT, "Indentified '%s' as a collection",
                   rods_path->outPath);
            type_arg = "-C";
            break;

        default:
            logmsg(ERROR, BATON_CAT,
                   "Failed to set metadata on '%s' as it is "
                   "neither data object nor collection",
                   rods_path->outPath);
            status = -1;
            goto error;
    }

    mod_metadata_in named_args;
    named_args.op = op;
    named_args.type_arg = type_arg;
    named_args.rods_path = rods_path;
    named_args.attr_name = attr_name;
    named_args.attr_value = attr_value;
    named_args.attr_units = attr_units;

    modAVUMetadataInp_t anon_args;
    map_mod_args(&anon_args, &named_args);
    status = rcModAVUMetadata(conn, &anon_args);
    if (status < 0)
        goto rods_error;

    return status;

rods_error:
    err_name = rodsErrorName(status, &err_subname);
    logmsg(ERROR, BATON_CAT,
           "Failed to add metadata '%s' -> '%s' to '%s': error %d %s %s",
           attr_name, attr_value, rods_path->outPath,
           status, err_name, err_subname);

    if (conn->rError)
        log_rods_errstack(ERROR, BATON_CAT, conn->rError);

error:
    return status;
}

void log_json_error(log_level level, const char* category,
                    json_error_t *error) {
    logmsg(level, category, "JSON error: %s, line %d, column %d, position %d",
           error->text, error->line, error->column, error->position);
}
