#!/bin/bash

set -ex

git config --global --add safe.directory "$PWD"

mkdir -p "$HOME/.irods"
cat <<'EOF' > "$HOME/.irods/irods_environment.json"
{
    "irods_host": "irods-server",
    "irods_port": 1247,
    "irods_user_name": "irods",
    "irods_zone_name": "testZone",
    "irods_home": "/testZone/home/irods",
    "irods_default_resource": "replResc"
}
EOF

nc -z -v irods-server 1247

echo "irods" | script -q -c "iinit" /dev/null
ienv
ils
ilsresc

export CK_DEFAULT_TIMEOUT: 60
export ASAN_OPTIONS: "detect_leaks=0"
# export LDFLAGS: ${{ matrix.irods == '4.3.0' && '-Wl,-z,muldefs' || '' }}

autoreconf -fi
./configure --enable-address-sanitizer
make distcheck DISTCHECK_CONFIGURE_FLAGS="--enable-address-sanitizer LDFLAGS=\"$LDFLAGS\""
