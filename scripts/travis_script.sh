#!/bin/bash

set -e -x

IRODS_HOME=
baton_irods_conf="--with-test-resource=testResc --with-irods"

if [ -n "$IRODS_RIP_DIR" ]
then
    export IRODS_HOME="$IRODS_RIP_DIR/iRODS"
    baton_irods_conf="--with-test-resource=testResc --with-irods=$IRODS_HOME"
fi

autoreconf -fi
./configure ${baton_irods_conf}
make distcheck DISTCHECK_CONFIGURE_FLAGS="${baton_irods_conf}"
