#!/bin/bash

set -ex

source "$CONDA_INSTALL_DIR/etc/profile.d/conda.sh"

conda activate base
conda config --add channels https://dnap.cog.sanger.ac.uk/npg/conda/devel/generic/
conda config --add channels https://dnap.cog.sanger.ac.uk/npg/conda/tools/generic/
