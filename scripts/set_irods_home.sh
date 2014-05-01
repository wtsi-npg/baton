#!/bin/bash

# Run `source set_irods_home.sh' to accept the location of the iRODS
# source tree as /usr/local/lib/irods.
#
# Run `IRODS_HOME=/my/path/to/iRODS source set_irods_home.sh' to use
# an iRODS source tree in an alternative location.

if [ -z "$IRODS_HOME" ]
then
    echo "Defaulting to iRODS home /usr/local/lib/irods"
    IRODS_HOME=/usr/local/lib/irods
else
    echo "Set iRODS home to $IRODS_HOME"
fi

CPPFLAGS="$CPPFLAGS \
-I$IRODS_HOME/lib/api/include \
-I$IRODS_HOME/lib/core/include \
-I$IRODS_HOME/lib/md5/include \
-I$IRODS_HOME/lib/sha1/include \
-I$IRODS_HOME/server/core/include \
-I$IRODS_HOME/server/drivers/include \
-I$IRODS_HOME/server/icat/include \
-I$IRODS_HOME/server/re/include"

LDFLAGS="$LDFLAGS -L$IRODS_HOME/lib/core/obj -lRodsAPIs"

export IRODS_HOME CPPFLAGS LDFLAGS
