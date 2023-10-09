#!/bin/bash

# This script wraps baton-do so that it will run under
# Valgrind. Symlink this script in place of your baton-do executable
# when testing your iRODS wrapper (e.g. perl-irods-wrap or extendo),
# making sure that the real baton-do it not on your PATH.
#
# You can then run your iRODS wrapper tests (or even higher level tests
# that use the wrapper), knowing that the baton code underneath is
# being tested for memory leaks.
#
# The iRODS libraries always generate some leak errors, so a Valgrind
# suppressions file is used to mute these.

set -e

LOG_DIR=${LOG_DIR:-"$PWD"}
SUPPRESSIONS=${SUPPRESSIONS:-"$PWD/suppressions/baton.supp"}
WRAPPED=${WRAPPED:-baton-do}

mkdir -p "$LOG_DIR"

valgrind --error-exitcode=1 \
         --leak-check=full \
         --show-leak-kinds=definite,possible \
         --suppressions="$SUPPRESSIONS" \
         --log-file="$LOG_DIR/baton-do.$BASHPID.log" \
         "$WRAPPED" -f /dev/stdin "$@" >/dev/stdout
