#!/bin/bash

set -e -u -x

sudo -H pip install --upgrade pip
sudo -H pip install 'sphinx==1.3.1'
sudo -H pip install 'breathe==3.2.0'

wget --quiet https://repo.anaconda.com/miniconda/Miniconda3-4.5.11-Linux-x86_64.sh -O ~/miniconda.sh

/bin/bash ~/miniconda.sh -b -p ~/miniconda
~/miniconda/bin/conda clean -tipsy
echo ". ~/miniconda/etc/profile.d/conda.sh" >> ~/.bashrc
echo "conda activate base" >> ~/.bashrc

. ~/miniconda/etc/profile.d/conda.sh
conda activate base
conda config --set auto_update_conda False
conda config --add channels https://dnap.cog.sanger.ac.uk/npg/conda/devel/generic/
conda create -y -n travis
conda activate travis
conda install -y libjansson-dev
conda install -y irods-dev
conda install -y irods-icommands

mkdir -p ~/.irods
cat <<EOF > ~/.irods/irods_environment.json
{
    "irods_host": "localhost",
    "irods_port": 1247,
    "irods_user_name": "irods",
    "irods_zone_name": "testZone",
    "irods_home": "/testZone/home/irods",
    "irods_plugins_home": "$HOME/miniconda/envs/travis/lib/irods/plugins/",
    "irods_default_resource": "testResc"
}
EOF

# Check
wget http://downloads.sourceforge.net/project/check/check/0.9.14/check-0.9.14.tar.gz -O /tmp/check-0.9.14.tar.gz
tar xfz /tmp/check-0.9.14.tar.gz -C /tmp
cd /tmp/check-0.9.14
autoreconf -fi
./configure ; make ; sudo make install

cd $TRAVIS_BUILD_DIR
sudo ldconfig
