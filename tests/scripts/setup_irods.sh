#!/bin/bash
#
# This script is run on checked fixture setup. It copies test data to
# iRODS and if provided with a file of imeta commands, executes imeta
# to add metadata.
#

E_ARGS_MISSING=3
E_INPUT_MISSING=4
E_OUTPUT_PRESENT=5
E_RESC_MISSING=6

in_path=$1
out_path=$2
test_resc=$3
meta_path=$4

if [ $# -lt 3 ]
then
    echo "Insufficient command line arguments; expected 3 or 4"
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

# Make replicates of test data
ilsresc $test_resc >&/dev/null
status=$?

if [[ $status -ne 0 ]]
then
    echo "Test resource '$test_resc' is missing. Aborting"
    exit $E_RESC_MISSING
fi

irepl -r -R $test_resc $out_path
status=$?

if [[ $status -ne 0 ]]
then
    echo "Failed to irepl test data to '$test_resc'"
    exit $status
else
    echo "Replicated test data to '$test_resc'"
fi

# Ensure checksums are up to date
ichksum -r -a -K $out_path >&/dev/null
status=$?

if [[ $status -ne 0 ]]
then
    echo "Failed to create checksums on test data. Aborting"
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
        sed -e "s#__IRODS_TEST_ROOT__#$out_path#" < $meta_path | imeta >/dev/null
        status=$?

        exit $status
    fi
fi
