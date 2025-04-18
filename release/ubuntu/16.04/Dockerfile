ARG BASE_IMAGE=ubuntu:16.04
FROM $BASE_IMAGE AS installer

ENV DEBIAN_FRONTEND=noninteractive

# libdeflate-dev is not available on Ubuntu 16.04
RUN apt-get update && \
    apt-get install -q -y --no-install-recommends \
        apt-transport-https \
        apt-utils \
        autoconf \
        automake \
        build-essential \
        ca-certificates \
        curl \
        dirmngr \
        gnupg \
        gnupg-agent \
        language-pack-en-base \
        libcurl3-dev \
        libjansson-dev \
        liblzma-dev \
        libtool \
        locales \
        lsb-release \
        pkg-config \
        python3-sphinx \
        zlib1g-dev && \
    locale-gen en_GB en_GB.UTF-8 && \
    localedef -i en_GB -c -f UTF-8 -A /usr/share/locale/locale.alias en_GB.UTF-8

ENV LANG=en_GB.UTF-8 \
    LANGUAGE=en_GB \
    LC_ALL=en_GB.UTF-8 \
    TZ=Etc/UTC

RUN curl -sSL https://packages.irods.org/irods-signing-key.asc | apt-key add - && \
    echo "deb [arch=amd64] https://packages.irods.org/apt/ $(lsb_release -sc) main" |\
    tee /etc/apt/sources.list.d/renci-irods.list

ARG IRODS_VERSION="4.2.7"

RUN apt-get update && \
    apt-get install -q -y --no-install-recommends \
        irods-dev="${IRODS_VERSION}" \
        irods-runtime="${IRODS_VERSION}"

RUN apt-key adv --keyserver keyserver.ubuntu.com --recv-keys "E1DD270288B4E6030699E45FA1715D88E1DF1F24" && \
    echo "deb https://ppa.launchpadcontent.net/git-core/ppa/ubuntu $(lsb_release -sc) main" |\
    tee /etc/apt/sources.list.d/git-core.list

RUN apt-get install -q -y --no-install-recommends \
        git

COPY --from=repo_root . /app

WORKDIR /app

ENV CPPFLAGS="-I/usr/include/irods"
ENV CPU_COUNT=4

RUN autoreconf -fi && \
    ./configure && \
    make -j ${CPU_COUNT} install && \
    ldconfig

FROM $BASE_IMAGE

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -q -y --no-install-recommends \
        apt-transport-https \
        apt-utils \
        ca-certificates \
        curl \
        dirmngr \
        gnupg \
        gnupg-agent \
        jq \
        language-pack-en-base \
        libcurl3 \
        libjansson4 \
        liblzma5 \
        locales \
        lsb-release \
        unattended-upgrades \
        zlib1g && \
    locale-gen en_GB en_GB.UTF-8 && \
    localedef -i en_GB -c -f UTF-8 -A /usr/share/locale/locale.alias en_GB.UTF-8

ENV LANG=en_GB.UTF-8 \
    LANGUAGE=en_GB \
    LC_ALL=en_GB.UTF-8 \
    TZ=Etc/UTC

RUN curl -sSL https://packages.irods.org/irods-signing-key.asc | apt-key add - && \
    echo "deb [arch=amd64] https://packages.irods.org/apt/ $(lsb_release -sc) main" |\
    tee /etc/apt/sources.list.d/renci-irods.list

ARG IRODS_VERSION

RUN apt-get update && \
    apt-get install -q -y --no-install-recommends \
        irods-runtime="${IRODS_VERSION}" && \
    unattended-upgrade -d -v && \
    apt-get remove -q -y unattended-upgrades && \
    apt-get autoremove -q -y && \
    apt-get clean -q -y && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the installed baton application
COPY --from=installer /usr/local /usr/local

# Copy the singularity-wrapper scripts
COPY --from=installer /app/release/singularity-* /usr/local/bin
COPY --from=installer /app/release/manifest.txt /usr/local/etc/baton/manifest.txt

ARG DOCKER_IMAGE
ARG DOCKER_TAG

# Configure the singularity-wrapper script
ENV MANIFEST_PATH="/usr/local/etc/baton/manifest.txt"
ENV DOCKER_IMAGE="$DOCKER_IMAGE"
ENV DOCKER_TAG="$DOCKER_TAG"

RUN ldconfig

ARG APP_USER=appuser
ARG APP_UID=1000
ARG APP_GID=$APP_UID

RUN groupadd --gid $APP_GID $APP_USER && \
    useradd --uid $APP_UID --gid $APP_GID --shell /bin/bash --create-home $APP_USER

USER $APP_USER

CMD ["/bin/bash"]
