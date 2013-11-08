#!/bin/sh
#
# This script is run on fixture setup. It copies test data to iRODS
# and if provided with a file of imeta commands, executes imeta to add
# metadata.
#

E_ARGS_MISSING=3
E_INPUT_MISSING=4
E_OUTPUT_PRESENT=5

in_path=$1
out_path=$2
meta_path=$3

if [ $# -lt 2 ]
then
    echo "Insufficient command line arguments; expected 2 or 3"
    exit $E_ARGS_MISSING
fi

# Is there anything already in the place we intend to write to in
# iRODS?
ils $out_path &>/dev/null
status=$?

if [[ $status -eq 0 ]]
then
    echo "Previous iRODS test data exists at '$out_path'. Aborting"
    exit $E_OUTPUT_PRESENT
fi

# Is there something to write to iRODS?
if [[ ! -d $in_path ]]
then
    echo "Test data input directory '$in_path' not found. Aborting"
    exit $E_INPUT_MISSING
fi

iput -r $in_path $out_path
status=$?

if [[ $status -ne 0 ]]
then
    echo "Failed to iput test data to '$out_path'"
    exit $status
fi

# Add metadata if required
if [[ ! -z "$meta_path" ]]
then
    if [[ ! -f $meta_path ]]
    then
        echo "Metadata input file '$meta_path' not found. Aborting"
        exit $E_INPUT_MISSING
    else
        sed -e "s/IRODS_TEST_ROOT/$out_path/" < $meta_path | imeta >/dev/null
        status=$?

        exit $status
    fi
fi
