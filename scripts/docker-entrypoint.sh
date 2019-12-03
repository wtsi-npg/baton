#!/bin/bash

set -ex

CONDA_USER_ID=${CONDA_USER_ID:-9001}
CONDA_USER=${CONDA_USER:-conda}
CONDA_GROUP_ID=${CONDA_GROUP_ID:-32001}
CONDA_INSTALL_DIR=${CONDA_INSTALL_DIR:-/opt/conda}

useradd --shell /bin/bash --uid $CONDA_USER_ID --gid $CONDA_GROUP_ID \
        --non-unique --create-home $CONDA_USER --comment ""

export USER=$CONDA_USER
export HOME=/home/$USER
export LOGNAME=$USER
export MAIL=/var/spool/mail/$USER
export PATH="$CONDA_INSTALL_DIR/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

if [ ! -e "$HOME/.condarc" ]; then
    cp "$CONDA_INSTALL_DIR/etc/condarc" "$HOME/.condarc"
fi

chown -R $USER:$USER /home/$USER
chmod g+w "$CONDA_INSTALL_DIR"

exec gosu $USER "$@"
