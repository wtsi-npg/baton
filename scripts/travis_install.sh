#!/bin/bash

set -e -x

sudo apt-get install -qq odbc-postgresql unixodbc-dev
sudo -H pip install --upgrade pip
sudo -H pip install sphinx
sudo -H pip install --upgrade sphinx
sudo -H pip install breathe
sudo -H pip install --upgrade breathe

# iRODS
wget -q https://github.com/wtsi-npg/disposable-irods/releases/download/${DISPOSABLE_IRODS_VERSION}/disposable-irods-${DISPOSABLE_IRODS_VERSION}.tar.gz -O /tmp/disposable-irods-${DISPOSABLE_IRODS_VERSION}.tar.gz
tar xfz /tmp/disposable-irods-${DISPOSABLE_IRODS_VERSION}.tar.gz -C /tmp
cd /tmp/disposable-irods-${DISPOSABLE_IRODS_VERSION}
./scripts/download_and_verify_irods.sh
./scripts/install_irods.sh
./scripts/configure_irods.sh

# Check
wget http://downloads.sourceforge.net/project/check/check/0.9.14/check-0.9.14.tar.gz -O /tmp/check-0.9.14.tar.gz
tar xfz /tmp/check-0.9.14.tar.gz -C /tmp
cd /tmp/check-0.9.14
autoreconf -fi
./configure ; make ; sudo make install

# Jansson
wget -q https://github.com/akheron/jansson/archive/v${JANSSON_VERSION}.tar.gz -O /tmp/jansson-${JANSSON_VERSION}.tar.gz
tar xfz /tmp/jansson-${JANSSON_VERSION}.tar.gz -C /tmp
cd /tmp/jansson-${JANSSON_VERSION}
autoreconf -fi
./configure ; make ; sudo make install

cd $TRAVIS_BUILD_DIR
sudo ldconfig
