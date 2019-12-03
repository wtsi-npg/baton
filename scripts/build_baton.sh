#!/bin/bash

set -ex

source "$CONDA_INSTALL_DIR/etc/profile.d/conda.sh"

conda build recipe
