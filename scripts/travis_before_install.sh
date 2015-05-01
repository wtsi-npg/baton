#!/bin/bash

set -e -x

IRODS_VERSION=${IRODS_VERSION:=3.3.1}

before_install_common() {
    sudo apt-get update -qq
}

before_install_3_3_1() {
    return
}

before_install_4_1_0() {
    IRODS_BUILD=${IRODS_VERSION}-${WTSI_NPG_SHA1}
    wget $WTSI_NPG_URL/releases/download/$IRODS_BUILD/irods-icat-${IRODS_VERSION}-64bit.deb
    wget $WTSI_NPG_URL/releases/download/$IRODS_BUILD/irods-database-plugin-postgres-${PG_PLUGIN_VERSION}.deb
    wget $WTSI_NPG_URL/releases/download/$IRODS_BUILD/irods-runtime-${IRODS_VERSION}-64bit.deb
    wget $WTSI_NPG_URL/releases/download/$IRODS_BUILD/irods-dev-${IRODS_VERSION}-64bit.deb
}

case $IRODS_VERSION in

    3.3.1)
        before_install_common
        before_install_3_3_1
        ;;

    4.1.0)
        before_install_common
        before_install_4_1_0
        ;;

    *)
        echo Unknown iRODS version $IRODS_VERSION
        exit 1
esac
