    # Update new packages
    FROM ubuntu:20.04

    LABEL maintainer="michael.casey@mara.com"
    LABEL version="0.1"
    LABEL description="Docker file for mara federation"

    ARG DEBIAN_FRONTEND=noninteractive

    RUN apt-get update
    RUN apt-get install -y \
        software-properties-common \
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
        unzip \
        git \
        wget \
        vim

    #Configure BerkeleyDB
    ENV BERKELEYDB_VERSION=db-4.8.30.NC
    ENV BERKELEYDB_PREFIX=/opt/${BERKELEYDB_VERSION}
    RUN wget https://download.oracle.com/berkeley-db/${BERKELEYDB_VERSION}.tar.gz
    RUN tar -xzf *.tar.gz
    RUN sed s/__atomic_compare_exchange/__atomic_compare_exchange_db/g -i ${BERKELEYDB_VERSION}/dbinc/atomic.h
    RUN mkdir -p ${BERKELEYDB_PREFIX}

    WORKDIR /${BERKELEYDB_VERSION}/build_unix

    RUN ../dist/configure --enable-cxx --disable-shared --with-pic --prefix=${BERKELEYDB_PREFIX}
    RUN make -j4
    RUN make install
    RUN rm -rf ${BERKELEYDB_PREFIX}/docs


    #Setup base directory
    RUN mkdir -p /opt/marachain
    WORKDIR /opt/marachain

    #Copy submodules
    COPY . ./

    #Configure sidechain node
    WORKDIR /opt/marachain
    RUN ./autogen.sh \
     && ./configure LDFLAGS=-L`ls -d /opt/db*`/lib/ CPPFLAGS=-I`ls -d /opt/db*`/include/ \
     && make && make install 

    WORKDIR /opt/marachain/src

    EXPOSE 19011
    EXPOSE 19010

    CMD ["sh","../entrypoint.sh"]