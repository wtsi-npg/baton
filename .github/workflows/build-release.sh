#!/bin/bash

set -ex

git config --global --add safe.directory "$PWD"

autoreconf -fi
./configure
make dist

dist_file=$(find . -name baton-\*.tar.gz -print0)
sha256sum "$dist_file" > "$dist_file".sha256

ls -l
