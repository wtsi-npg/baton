#!/bin/bash

set -e -x

IRODS_VERSION=${IRODS_VERSION:=3.3.1}

install_common() {
    sudo apt-get install -qq odbc-postgresql unixodbc-dev
    sudo apt-get install -qq python-sphinx
    sudo -H pip install breathe

    wget http://downloads.sourceforge.net/project/check/check/0.9.14/check-0.9.14.tar.gz -O /tmp/check-0.9.14.tar.gz
    tar xfz /tmp/check-0.9.14.tar.gz -C /tmp
    cd /tmp/check-0.9.14
    autoreconf -fi
    ./configure ; make ; sudo make install

    wget -q https://github.com/akheron/jansson/archive/v${JANSSON_VERSION}.tar.gz -O /tmp/jansson-${JANSSON_VERSION}.tar.gz
    tar xfz /tmp/jansson-${JANSSON_VERSION}.tar.gz -C /tmp
    cd /tmp/jansson-${JANSSON_VERSION}
    autoreconf -fi
    ./configure ; make ; sudo make install

    cd $TRAVIS_BUILD_DIR
    sudo ldconfig
}

install_3_3_1() {
    return
}

install_4_1_x() {
    sudo apt-get install -qq python-psutil python-requests
    sudo apt-get install -qq python-sphinx
    sudo apt-get install super libjson-perl jq
    sudo -H pip install jsonschema

    sudo dpkg -i irods-icat-${IRODS_VERSION}-${PLATFORM}-${ARCH}.deb irods-database-plugin-postgres-${PG_PLUGIN_VERSION}-${PLATFORM}-${ARCH}.deb
    sudo dpkg -i irods-runtime-${IRODS_VERSION}-${PLATFORM}-${ARCH}.deb irods-dev-${IRODS_VERSION}-${PLATFORM}-${ARCH}.deb
}

case $IRODS_VERSION in

    3.3.1)
        install_common
        install_3_3_1
        ;;

    4.1.9)
        install_common
        install_4_1_x
        ;;

    *)
        echo Unknown iRODS version $IRODS_VERSION
        exit 1
esac
