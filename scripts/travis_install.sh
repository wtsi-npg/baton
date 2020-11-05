#!/bin/bash

set -e -u -x

wget --quiet https://repo.anaconda.com/miniconda/Miniconda3-4.6.14-Linux-x86_64.sh -O ~/miniconda.sh

/bin/bash ~/miniconda.sh -b -p ~/miniconda
~/miniconda/bin/conda clean -tipsy
echo ". ~/miniconda/etc/profile.d/conda.sh" >> ~/.bashrc
echo "conda activate base" >> ~/.bashrc

. ~/miniconda/etc/profile.d/conda.sh
conda activate base
conda config --set auto_update_conda False

conda create -y -n travis
conda activate travis
conda install -y python=3.8
pip install sphinx==2.4.0

conda config --add channels https://dnap.cog.sanger.ac.uk/npg/conda/devel/generic/
conda config --append channels conda-forge

conda install -y check
conda install -y libjansson-dev
conda install -y irods-dev="$IRODS_VERSION"
conda install -y irods-icommands="$IRODS_VERSION"

mkdir -p ~/.irods
cat <<EOF > ~/.irods/irods_environment.json
{
    "irods_host": "localhost",
    "irods_port": 1247,
    "irods_user_name": "irods",
    "irods_zone_name": "testZone",
    "irods_home": "/testZone/home/irods",
    "irods_plugins_home": "$HOME/miniconda/envs/travis/lib/irods/plugins/",
    "irods_default_resource": "replResc"
}
EOF

