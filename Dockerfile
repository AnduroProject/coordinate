    FROM ubuntu:20.04

    LABEL maintainer="michael.casey@mara.com"
    LABEL version="0.1"
    LABEL description="Docker file for mara federation"

    ARG DEBIAN_FRONTEND=noninteractive

    # Update new packages
    RUN apt update
    RUN apt-get install -y \
        build-essential \
        libtool autotools-dev \
        automake \
        curl \
        libssl-dev \
        pkg-config \
        libtool \
        libevent-dev \
        python3 \
        autoconf \
        automake \
        libzmq3-dev \
        libsqlite3-dev \
        libboost-all-dev \ 
        bsdmainutils \
        net-tools \
        git \
        wget \
        vim
    RUN apt-get update


    #setup base directory
    RUN mkdir -p /marachain
    WORKDIR /marachain

    #Copy submodules
    COPY . ./

    #configure sidechain node
    WORKDIR /marachain
    RUN ./autogen.sh \
     && ./configure \
     && make && make install 

    WORKDIR /marachain/src
    CMD ["./bitcoind"]