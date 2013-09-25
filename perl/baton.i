
%module baton
%{

#include "rcConnect.h"
#include "rodsClient.h"
#include "rodsPath.h"

#include "baton.h"

%}

%ignore json_vpack_ex;
%ignore json_vunpack_ex;

%include "rcConnect.h"
%include "rodsClient.h"
%include "rodsPath.h"

%include "jansson_config.h"
%include "jansson.h"

%include "baton.h"
