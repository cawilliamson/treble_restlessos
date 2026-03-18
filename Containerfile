FROM ubuntu:24.04

# Set non-interactive installation
ENV DEBIAN_FRONTEND=noninteractive

# Set environment variables
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8
ENV TZ=UTC

# Install required packages
RUN apt-get update && apt-get install -y \
    bc \
    bison \
    build-essential \
    ccache \
    curl \
    flex \
    g++-multilib \
    gcc-multilib \
    git \
    git-lfs \
    gnupg \
    gperf \
    imagemagick \
    lib32readline-dev \
    lib32z1-dev \
    liblz4-tool \
    libsdl1.2-dev \
    libssl-dev \
    libxml2 \
    libxml2-utils \
    lzop \
    openjdk-11-jdk \
    pngcrush \
    python3 \
    python3-pip \
    rsync \
    schedtool \
    squashfs-tools \
    sudo \
    unzip \
    wget \
    xsltproc \
    xz-utils \
    zip \
    zlib1g-dev \
    zsync \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Create directory structure
RUN mkdir -p /repo

# install libncurses5
RUN cd /var/tmp && \
    curl -O http://launchpadlibrarian.net/648013231/libtinfo5_6.4-2_amd64.deb && \
    dpkg -i libtinfo5_6.4-2_amd64.deb && \
    curl -LO http://launchpadlibrarian.net/648013227/libncurses5_6.4-2_amd64.deb && \
    dpkg -i libncurses5_6.4-2_amd64.deb && \
    rm -f ./*.deb

# Install repo tool
RUN curl -o /usr/local/bin/repo https://storage.googleapis.com/git-repo-downloads/repo \
    && chmod a+x /usr/local/bin/repo

# Configure git
RUN git config --global user.email 'androidbuild@localhost' && \
    git config --global user.name 'androidbuild'

# Set up working directory
WORKDIR /repo
