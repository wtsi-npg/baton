#!/usr/bin/env bash

usage() {
    cat 1>&2 <<EOF 

This script creates a bad replica of an object in docker iRODs
server by marking a replica as stale, giving it an incorrect checksum
in the database, and/or changing the contents without informing iRODs.

Usage: $(basename "$0")
       [-c <collection path>]
       [-d <object name>] 
       [-r <replica number>]
       [-i <irods server name>]
       [-s] [-a] [-o] 
       [-v]
       [-h]

Options
	-h Display usage and exit
	-v verbose
	-c path to collection
	-d data object in name
	-r replica number to alter
	-i irods server container name
	-s mark as stale
	-a alter checksum
	-o corrupt object contents

EOF
}

log () {
    if [[ $verbose == 1 ]] ; then
        printf "${1}"
    fi
}

ils_error() {
    echo "Error running ils, object may not exist or iinit may not have been run"
    exit 1
}   

#Default values
repl_num=1
irods_container="irods-server"

while getopts "hvc:d:r:i:sao" option; do
    case "$option" in
	h)
	    usage
	    exit 0
	    ;;
	v)
	    verbose=1
	    ;;
	c)
	    coll="$OPTARG"
	    ;;
	d)
	    obj="$OPTARG"
	    ;;
	r)
	    repl_num="$OPTARG"
	    ;;
	i)
	    irods_container="$OPTARG"
	    ;;
	s)
	    stale=1
	    ;;
	a)
	    check=1
	    ;;
	o)
	    corrupt=1
	    ;;
       
    esac
done

if [[ -z $coll ]] ; then
    printf "Collection path required"
    usage
    exit 1
elif [[ -z $obj ]] ; then
    printf "Object name required"
    usage
    exit 1
else
    exists=$(ils "$coll/$obj") || ils_error
fi

log "Object: $coll/$obj \nReplica: $repl_num \nOperations: \n"
updates=""

if [[ -n $stale ]] ; then
    log "  - make stale \n"
    updates+="DATA_IS_DIRTY='0' "
    op=1
fi

if [[ -n $check ]] ; then
    log "  - alter checksum \n"
    updates+="DATA_CHECKSUM='0' "    
    op=1
fi

if [[ -n $corrupt ]] ; then
    log "  - corrupt data \n"
    fs_path=$(ils -L $coll/$obj | grep /var/lib/irods | sed "$(($repl_num+1))q;d" | sed -e 's/.*generic   //g')
    docker exec -u root "$irods_container" bash -c "echo 'This file is corrupted' > $fs_path" || exit 1;
    op=2
fi

if [[ -z $op ]]; then
    printf "No operation requested \n"
    exit 0
fi


if [[ $op -eq 1 ]]; then
    docker exec -u root "$irods_container" sudo -u irods psql -d ICAT -c "UPDATE r_data_main d SET $updates FROM r_coll_main c WHERE d.coll_id = c.coll_id AND d.DATA_NAME='$obj' AND c.COLL_NAME='$coll' AND d.DATA_REPL_NUM='$repl_num';" || exit 1
fi


