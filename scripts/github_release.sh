#!/bin/bash
#
# Copyright (C) 2017 Genome Research Limited. All Rights Reserved.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the Perl Artistic License or the GNU General
# Public License as published by the Free Software Foundation, either
# version 3 of the License, or (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# Author: Keith James

# Needs error handling to report better messages when HTTP responses
# indicate a failure on the server

set -e -o pipefail

# trap cleanup EXIT INT TERM

# cleanup() {
#     rm -r ${TMP}
# }

usage() {
    cat 1>&2 << EOF

This script creates a release on GitHub, creates a sha256 checksum of
each release artifact and uploads the artifacts and checksum files to
the release.

A GitHub API token is required to be stored locally in a file
accessible to the script. The token is sent to GitHub using SSL.

Version: $VERSION
Author:  Keith James <kdj@sanger.ac.uk>

Usage: $0 -a <GitHub API token file> \\
  -b "<release description body>" \\
  -c <target commitish> [-d] -n <release name> [-p] \\
  -r <GitHub repository name> -t <release tag name> \\
  -u <GitHub user name> [-v] <release artifact files>

Options:
 -b Release description body. Required.
 -c Target commitish e.g. 'master'. Required.
 -d Draft release flag
 -n Release name. Required.
 -p Pre-release flag
 -r Github repository name. Required.
 -t Tag name e.g. '1.0.0'. Required.
 -u GitHub user name. Required.
 -v Print verbose messages
EOF
}

# Read the API token from a file
read_api_token() {
    while read token || [[ -n $token ]] ; do
        TOKEN=${TOKEN:-$token}
    done < $GITHUB_API_TOKEN
}

# URL encode an input
urlencode() {
    echo $(python -c "import urllib ; print(urllib.quote(\"$1\"))")
}

# Create the release and extract the upload URL from the response
create_github_release() {
    pushd ${TMP}

    cat > release.json <<EOF
{"tag_name":         "$TAG_NAME",
 "target_commitish": "$TARGET_COMMITISH",
 "name":             "$RELEASE_NAME",
 "body":             "$RELEASE_BODY",
 "draft":            $DRAFT,
 "prerelease":       $PRERELEASE}
EOF

    $CURL -u $TOKEN:x-oauth-basic -X POST \
          -sSL https://api.github.com/repos/$USER_NAME/$REPOSITORY_NAME/releases \
          -d @release.json > response.json

    GITHUB_UPLOAD_URL=$($PYTHON -c 'import json ; print(json.load(open("response.json"))["upload_url"])' | sed -e 's/{?name,label}//')

    popd
}

# Checksum and upload the artifacts to the upload URL obtained from
# the release response
upload_release_artifacts() {
    for artifact in "${RELEASE_ARTIFACTS[@]}"; do
        if [ ! -e "$artifact" ] ; then
            echo "Release artifact $artifact file does not exist"
            exit 5
        fi

        shasum -a 256 "$artifact" > "$artifact.sha256"

        encoded=$(urlencode "$artifact")

        $CURL -u $TOKEN:x-oauth-basic -X POST \
              -H "Content-Type: application/octet-stream" \
              -sSL $GITHUB_UPLOAD_URL?name="$encoded" \
              --data-binary @"$artifact"

        $CURL -u $TOKEN:x-oauth-basic -X POST \
              -H "Content-Type: application/octet-stream" \
              -sSL $GITHUB_UPLOAD_URL?name="$encoded.sha256" \
              --data-binary @"$artifact.sha256"
    done
}

CURL=/usr/bin/curl
PYTHON=python

TMPDIR=$PWD/
TMP=$(mktemp -d ${TMPDIR:-/tmp/}$(basename -- "$0").XXXXXXXXXX)

GITHUB_API_URL=https://api.github.com
GITHUB_API_TOKEN=
GITHUB_UPLOAD_URL=

RELEASE_BODY=
TARGET_COMMITISH=
DRAFT=false
RELEASE_NAME=
PRERELEASE=false
REPOSITORY_NAME=
TAG_NAME=
USER_NAME=

while getopts "a:b:c:dn:pr:t:u:v" option; do
    case "$option" in
        a)
            GITHUB_API_TOKEN="${OPTARG}"
            ;;
        b)
            RELEASE_BODY="${OPTARG}"
            ;;
        c)
            TARGET_COMMITISH="${OPTARG}"
            ;;
        d)
            DRAFT="true"
            ;;
        n)
            RELEASE_NAME="${OPTARG}"
            ;;
        p)
            PRERELEASE="true"
            ;;
        r)
            REPOSITORY_NAME="${OPTARG}"
            ;;
        t)
            TAG_NAME="${OPTARG}"
            ;;
        u)
            USER_NAME="${OPTARG}"
            ;;
        v)
            set -x
            ;;
        *)
            usage
            echo "Invalid argument!"
            exit 4
            ;;
    esac
done

shift "$((OPTIND-1))"

RELEASE_ARTIFACTS=("$@")

if [ -z "$GITHUB_API_TOKEN" ] ; then
    usage
    echo -e "\nERROR:\n  A -a <API token file> argument is required"
    exit 4
fi

if [ -z "$RELEASE_BODY" ] ; then
    usage
    echo -e "\nERROR:\n  A -b <release body> argument is required"
    exit 4
fi

if [ -z "$TARGET_COMMITISH" ] ; then
    usage
    echo -e "\nERROR:\n  A -c <target commitish> argument is required"
    exit 4
fi

if [ -z "$RELEASE_NAME" ] ; then
    usage
    echo -e "\nERROR:\n  A -n <release name> argument is required"
    exit 4
fi

if [ -z "$REPOSITORY_NAME" ] ; then
    usage
    echo -e "\nERROR:\n  A -r <repository name> argument is required"
    exit 4
fi

if [ -z "$TAG_NAME" ] ; then
    usage
    echo -e "\nERROR:\n  A -t <tag name> argument is required"
    exit 4
fi

if [ -z "$USER_NAME" ] ; then
    usage
    echo -e "\nERROR:\n  A -u <user name> argument is required"
    exit 4
fi

if [ -z "$RELEASE_ARTIFACTS" ] ; then
    usage
    echo -e "\nERROR:\n  No release artifacts were supplied"
    exit 4
fi

read_api_token
create_github_release
upload_release_artifacts
