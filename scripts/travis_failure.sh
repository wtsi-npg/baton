#!/bin/bash

. ~/miniconda/etc/profile.d/conda.sh
conda activate travis
ilsresc
ils -Lr
cat ./baton*/_build/*/tests/check_baton.log
