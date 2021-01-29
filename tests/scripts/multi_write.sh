#! /usr/bin/env bash

if [ "$1" == "" ]; then
	echo "Please provide an irods collection path to put the files into,\
	it is suggested that this collection path does not currently exist to make it easier to delete once the test is complete" && exit 1
fi

# produce temp file
TMPFILE=$(mktemp) # TMPDIR can be set if the file should be placed elsewhere
dd if=/dev/zero of=$TMPFILE bs=1M count=10 > /dev/null 2>&1

# make collection
imkdir -p $1

i=0
while [ $i -lt 10 ] ; do

	json=$(jq -n "{collection: \"$1\", data_object: \"$(basename $TMPFILE)_$i.txt\", directory: \"$(dirname $TMPFILE)\", file: \"$(basename $TMPFILE)\"}")

	echo $json | baton-put > /dev/null 2>&1 & echo $json | baton-put > /dev/null 2>&1
	
	((i++))
done

# show files to demonstrate which have zero size
ils -l $1 || (echo "could not run ils, please check that you have initialised iRODS" && exit 1)
