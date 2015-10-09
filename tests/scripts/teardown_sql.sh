#!/bin/bash
#
# This script is run on unchecked fixture teardown. It removes iRODS
# specific query SQL if rodsadmin is available.
#

E_ARGS_MISSING=3
E_INPUT_MISSING=4

sql_path=$1

if [[ $# < 1 ]]
then
    echo "Insufficient command line arguments; expected 1"
    exit $E_ARGS_MISSING
fi

if [[ ! -z "$sql_path" ]]
then
    if [[ ! -f $sql_path ]]
    then
        echo "SQL input file '$sql_path' not found. Aborting"
        exit $E_INPUT_MISSING
    else
        # Test for admin capabilities
        iuserinfo | grep 'type: rodsadmin' >/dev/null
        status=$?

        if [[ $status -eq 0 ]]
        then
            echo "# Removing specific queries"
            awk '{print "rsq $s"}' < $sql_path | iadmin >/dev/null
            status=$?

            if [[ $status -ne 0 ]]
            then
                echo "Failed to remove SQL in '$sql_path'"
                exit $status
            fi
        else
            echo "# Not removing specific queries"
        fi
    fi
fi
