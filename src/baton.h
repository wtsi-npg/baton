
#include <stdarg.h>

#include "rcConnect.h"
#include "rodsPath.h"

#define BATON_CAT "baton"
#define BMETA_CAT "baton.bmeta"

typedef enum {
    FATAL,
    ERROR,
    WARN,
    NOTICE,
    INFO,
    DEBUG
} log_level;

typedef enum {
    META_ADD,
    META_REM,
} metadata_op;

static char *metadata_ops[] = {
     "add",
     "rem",
};

typedef struct {
    metadata_op op;
    char *type_arg;
    rodsPath_t *rods_path;
    char *attr_name;
    char *attr_value;
    char *attr_units;
} mod_metadata_in;

void logmsg(log_level level, const char* category, const char *format, ...);

void log_rods_errstack(log_level level, const char* category, rError_t *Err);

rcComm_t *rods_login(rodsEnv *env);

int init_rods_path(rodsPath_t *rodspath, char *inpath);

int resolve_rods_path(rcComm_t *conn, rodsEnv *env,
                      rodsPath_t *rods_path, char *inpath);

int modify_metadata(rcComm_t *conn, rodsPath_t *rodspath, metadata_op op,
                    char *attr_name, char *attr_value, char *attr_unit);


int printRodsPath(rodsPath_t *path);

int isLeaf(collEnt_t *collEnt);

int zombat(rcComm_t *conn, rodsPath_t *srcPath);

int listPath(rcComm_t *conn, rodsPath_t *srcPath,
             int (*callback) (rodsPath_t *));
