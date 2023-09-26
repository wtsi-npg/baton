ARG BASE_IMAGE=ubuntu:16.04
FROM $BASE_IMAGE

ARG IRODS_VERSION="4.2.7"

RUN echo 'debconf debconf/frontend select Noninteractive' | debconf-set-selections && \
    apt-get update && \
    apt-get install -q -y --no-install-recommends \
    apt-transport-https \
    apt-utils \
    ca-certificates \
    curl \
    dirmngr \
    gnupg \
    gnupg-agent \
    lsb-release \
    locales && \
    locale-gen en_GB en_GB.UTF-8 && \
    localedef -i en_GB -c -f UTF-8 -A /usr/share/locale/locale.alias en_GB.UTF-8

ENV LANG=en_GB.UTF-8 \
    LANGUAGE=en_GB \
    LC_ALL=en_GB.UTF-8

RUN curl -sSL https://packages.irods.org/irods-signing-key.asc | apt-key add - && \
    echo "deb [arch=amd64] https://packages.irods.org/apt/ $(lsb_release -sc) main" |\
    tee /etc/apt/sources.list.d/renci-irods.list && \
    apt-get update && \
    apt-get install -q -y --no-install-recommends \
    irods-dev="${IRODS_VERSION}" \
    irods-runtime="${IRODS_VERSION}" \
    irods-icommands="${IRODS_VERSION}"

RUN apt-key adv --keyserver keyserver.ubuntu.com --recv-keys "E1DD270288B4E6030699E45FA1715D88E1DF1F24" && \
    echo "deb https://ppa.launchpadcontent.net/git-core/ppa/ubuntu $(lsb_release -sc) main" |\
    tee /etc/apt/sources.list.d/git-core.list && \
    apt-get update && \
    apt-get install -q -y --no-install-recommends \
    git

RUN apt-get update && \
    apt-get install -q -y --no-install-recommends \
    autoconf \
    automake \
    build-essential \
    check \
    cmake \
    gdb \
    jq \
    lcov \
    less \
    libjansson-dev \
    libtool \
    netcat \
    pkg-config \
    python3-sphinx \
    ssh \
    valgrind \
    unattended-upgrades && \
    unattended-upgrade -v

ENV CPPFLAGS="-I/usr/include/irods" \
    CK_DEFAULT_TIMEOUT=20

CMD ["/bin/bash"]
