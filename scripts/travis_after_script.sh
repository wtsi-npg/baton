#!/bin/bash

set -e -x

IRODS_VERSION=${IRODS_VERSION:=3.3.1}

case $IRODS_VERSION in

    3.3.1)
        $TRAVIS_BUILD_DIR/irodscontrol istop
        ;;

    4.1.9)
        ;;

    *)
        echo Unknown iRODS version $IRODS_VERSION
        exit 1
esac
