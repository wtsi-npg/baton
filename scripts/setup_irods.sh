#!/bin/bash

set -ex

mkdir -p $HOME/.irods
cat <<EOF > $HOME/.irods/irods_environment.json
{
    "irods_host": "irods-service",
    "irods_port": 1247,
    "irods_user_name": "irods",
    "irods_zone_name": "testZone",
    "irods_home": "/testZone/home/irods",
    "irods_plugins_home": "$CONDA_INSTALL_DIR/envs/github/lib/irods/plugins/",
    "irods_default_resource": "testResc"
}
EOF

source $CONDA_INSTALL_DIR/etc/profile.d/conda.sh

conda activate github

echo "irods" | script -q -c "iinit" /dev/null
ienv
ils
