
%module BatonWrap
%{

#include "rodsDef.h"
#include "getRodsEnv.h"
#include "rcConnect.h"
#include "rodsClient.h"
#include "rodsPath.h"
#include "zlog.h"

#include "baton.h"

%}

/* zlog */
int zlog_init(const char *confpath);
int zlog_reload(const char *confpath);
void zlog_fini(void);

/* Jansson */
%ignore json_vpack_ex;
%ignore json_vunpack_ex;

%include "jansson_config.h"
%include "jansson.h"

/* iRODS */
%include "getRodsEnv.h"
%include "rcConnect.h"
%include "rodsClient.h"
%include "rodsPath.h"

/* baton */
%include "baton.h"
