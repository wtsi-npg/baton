#!/bin/bash
#
# This script is run on checked fixture teardown. It deletes the test
# data from iRODS on a successful result.
#

E_ARGS_MISSING=3

owner="$1"
in_path="$2"

if [[ $# < 2 ]]
then
    echo "Insufficient command line arguments; expected 2"
    exit $E_ARGS_MISSING
fi

ichmod -r own "$owner" "$in_path"
irm -rf "$in_path"

exit
