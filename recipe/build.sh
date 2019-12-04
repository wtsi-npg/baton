#!/bin/sh

set -ex

n="$CPU_COUNT"

mkdir -p $HOME/.irods
cat <<EOF > $HOME/.irods/irods_environment.json
{
    "irods_host": "irods-service",
    "irods_port": 1247,
    "irods_user_name": "irods",
    "irods_zone_name": "testZone",
    "irods_home": "/testZone/home/irods",
    "irods_plugins_home": "$PREFIX/lib/irods/plugins/",
    "irods_default_resource": "testResc"
}
EOF

echo "irods" | script -q -c "iinit" /dev/null
ienv
ils

autoreconf -fi

export LD_LIBRARY_PATH="$PREFIX/lib"
export CPPFLAGS="-I$PREFIX/include -I$PREFIX/include/irods"
export LDFLAGS="-L$PREFIX/lib -L$PREFIX/lib/irods/externals"

./configure --prefix="$PREFIX" --with-test-resource=demoResc \
            CPPFLAGS="$CPPFLAGS" LDFLAGS="$LDFLAGS"

export CK_DEFAULT_TIMEOUT=20
make -j $n
make check
make install

make -j $n distcheck DISTCHECK_CONFIGURE_FLAGS="--with-test-resource=demoResc CPPFLAGS=\"$CPPFLAGS\" LDFLAGS=\"$LDFLAGS\""
