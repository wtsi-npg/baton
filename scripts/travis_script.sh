#!/bin/bash

set -e -x

. ~/miniconda/etc/profile.d/conda.sh
conda activate travis

echo "irods" | script -q -c "iinit" /dev/null
ienv
ils

CONDA_ENV=/home/travis/miniconda/envs/travis
CPPFLAGS="-I$CONDA_ENV/include -I$CONDA_ENV/include/irods"
LDFLAGS="-L$CONDA_ENV/lib -L$CONDA_ENV/lib/irods/externals"

autoreconf -fi

./configure --with-test-resource=testResc \
            CPPFLAGS="$CPPFLAGS" LDFLAGS="$LDFLAGS"

export LD_LIBRARY_PATH="$CONDA_ENV/lib"

make distcheck DISTCHECK_CONFIGURE_FLAGS="--with-test-resource=testResc CPPFLAGS=\"$CPPFLAGS\" LDFLAGS=\"$LDFLAGS\""
