#!/bin/bash

set -e -x

IRODS_VERSION=${IRODS_VERSION:=3.3.1}

before_script_common() {
    return
}

before_script_3_3_1() {
    cd $TRAVIS_BUILD_DIR
    wget https://github.com/wtsi-npg/irods-legacy/releases/download/3.3.1-travis-bc85aa/irods.tar.gz -O /tmp/irods.tar.gz
    tar xfz /tmp/irods.tar.gz

    source $TRAVIS_BUILD_DIR/travis_linux_env.sh
    export IRODS_HOME=$TRAVIS_BUILD_DIR/iRODS
    export PATH=$PATH:$IRODS_HOME/clients/icommands/bin
    export IRODS_VAULT=/usr/local/var/lib/irods/Vault
    export IRODS_TEST_VAULT=/usr/local/var/lib/irods/Test

    sudo mkdir -p $IRODS_VAULT
    sudo chown $USER:$USER $IRODS_VAULT

    sudo mkdir -p $IRODS_TEST_VAULT
    sudo chown $USER:$USER $IRODS_TEST_VAULT

    sudo -E -u postgres $TRAVIS_BUILD_DIR/setup_pgusers.sh
    sudo -E -u postgres $TRAVIS_BUILD_DIR/irodscontrol psetup
    $TRAVIS_BUILD_DIR/irodscontrol istart ; sleep 10

    echo irods | script -q -c "iinit"
    iadmin mkresc testResc 'unix file system' cache `hostname --fqdn` $IRODS_TEST_VAULT
}

before_script_4_1_x() {
    sudo -E -u postgres createuser -D -R -S irods
    sudo -E -u postgres createdb -O irods ICAT
    sudo -E -u postgres sh -c "echo \"ALTER USER irods WITH PASSWORD 'irods'\" | psql"
    sudo /var/lib/irods/packaging/setup_irods.sh < .travis.setup_irods
    sudo jq -f .travis.server_config /etc/irods/server_config.json > server_config.tmp
    sudo mv server_config.tmp /etc/irods/server_config.json
    ls -l /etc/irods
    sudo /etc/init.d/irods restart
    sudo -E su irods -c "iadmin mkuser $USER rodsadmin ; iadmin moduser $USER password testuser"
    sudo -E su irods -c "iadmin lu $USER"
    sudo -E su irods -c "mkdir -p /var/lib/irods/iRODS/Test"
    sudo -E su irods -c "iadmin mkresc testResc unixfilesystem `hostname --fqdn`:/var/lib/irods/iRODS/Test"
    mkdir $HOME/.irods
    sed -e "s#__USER__#$USER#" -e "s#__HOME__#$HOME#" < .travis.irodsenv.json > $HOME/.irods/irods_environment.json
    cat $HOME/.irods/irods_environment.json
    ls -la $HOME/.irods/
    echo testuser | script -q -c "iinit"
    ls -la $HOME/.irods/
}

case $IRODS_VERSION in

    3.3.1)
        before_script_common
        before_script_3_3_1
        ;;

    4.1.4)
        before_script_common
        before_script_4_1_x
        ;;

    4.1.5)
        before_script_common
        before_script_4_1_x
        ;;

    4.1.6)
        before_script_common
        before_script_4_1_x
        ;;

    *)
        echo Unknown iRODS version $IRODS_VERSION
        exit 1
esac
