#!/bin/bash

set -e -x

IRODS_VERSION=${IRODS_VERSION:=3.3.1}

before_install_common() {
    sudo apt-get update -qq
}

before_install_3_3_1() {
    return
}

before_install_4_1_x() {
    wget ${RENCI_URL}/pub/irods/releases/${IRODS_VERSION}/${PLATFORM}/irods-icat-${IRODS_VERSION}-${PLATFORM}-${ARCH}.deb
    wget ${RENCI_URL}/pub/irods/releases/${IRODS_VERSION}/${PLATFORM}/irods-database-plugin-postgres-${PG_PLUGIN_VERSION}-${PLATFORM}-${ARCH}.deb
    wget ${RENCI_URL}/pub/irods/releases/${IRODS_VERSION}/${PLATFORM}/irods-runtime-${IRODS_VERSION}-${PLATFORM}-${ARCH}.deb
    wget ${RENCI_URL}/pub/irods/releases/${IRODS_VERSION}/${PLATFORM}/irods-dev-${IRODS_VERSION}-${PLATFORM}-${ARCH}.deb
}

case $IRODS_VERSION in

    3.3.1)
        before_install_common
        before_install_3_3_1
        ;;

    4.1.8)
        before_install_common
        before_install_4_1_x
        ;;

    *)
        echo Unknown iRODS version $IRODS_VERSION
        exit 1
esac
