FROM ubuntu:bionic

RUN echo 'debconf debconf/frontend select Noninteractive' | debconf-set-selections && \
    apt-get update && \
    apt-get install -q -y --no-install-recommends \
    apt-utils \
    ca-certificates \
    curl \
    gpg \
    gpg-agent \
    lsb-release \
    locales && \
    locale-gen en_GB en_GB.UTF-8 && \
    localedef -i en_GB -c -f UTF-8 -A /usr/share/locale/locale.alias en_GB.UTF-8

ENV LANG=en_GB.UTF-8 \
    LANGUAGE=en_GB \
    LC_ALL=en_GB.UTF-8

ENV IRODS_VERSION=4.2.11

RUN curl -sSL https://packages.irods.org/irods-signing-key.asc | apt-key add - && \
    echo "deb [arch=amd64] https://packages.irods.org/apt/ $(lsb_release -sc) main" |\
    tee /etc/apt/sources.list.d/renci-irods.list && \
    apt-get update && \
    apt-get install -q -y --no-install-recommends \
    irods-dev="${IRODS_VERSION}-1~$(lsb_release -sc)" \
    irods-runtime="${IRODS_VERSION}-1~$(lsb_release -sc)" \
    irods-icommands="${IRODS_VERSION}-1~$(lsb_release -sc)"

RUN apt-get update && \
    apt-get install -q -y --no-install-recommends \
    autoconf \
    automake \
    build-essential \
    check \
    gdb \
    git \
    jq \
    lcov \
    less \
    libjansson-dev \
    libtool \
    pkg-config \
    ssh \
    valgrind \
    unattended-upgrades && \
    unattended-upgrade -d -v

ARG USER=baton
ARG UID=1000
ARG GID=$UID

RUN groupadd --gid $GID $USER && \
    useradd --uid $UID --gid $GID --shell /bin/bash --create-home $USER

USER $USER

ENV CPPFLAGS="-I/usr/include/irods" \
    CK_DEFAULT_TIMEOUT=20

CMD ["/bin/bash"]
