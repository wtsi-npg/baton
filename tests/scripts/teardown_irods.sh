#!/bin/bash
#
# This script is run on fixture teardown. It deletes the test data
# from iRODS on a successful result.
#

E_ARGS_MISSING=3

in_path=$1

if [[ $# < 1 ]]
then
    echo "Insufficient command line arguments; expected 1"
    exit $E_ARGS_MISSING
fi

irm -rf $in_path
exit
