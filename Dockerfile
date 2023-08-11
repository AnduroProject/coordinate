    # Update new packages
    FROM ubuntu:22.04

    LABEL maintainer="michael.casey@mara.com"
    LABEL version="0.1"
    LABEL description="Docker file for mara federation"

    ARG DEBIAN_FRONTEND=noninteractive

    RUN apt-get update
    RUN apt-get install -y \
        build-essential \
        libtool \
        autotools-dev \
        automake \
        pkg-config \
        bsdmainutils \
        python3 \
        libevent-dev \
        libboost-dev \
        libsqlite3-dev \
        libminiupnpc-dev \
        libnatpmp-dev \
        libzmq3-dev \
        systemtap-sdt-dev 

    #configure nodejs 16.x
    RUN curl -s https://deb.nodesource.com/setup_16.x | bash
    RUN apt install nodejs -y
    RUN apt install npm -y
    

    #Setup base directory
    RUN mkdir -p /opt/marachain
    WORKDIR /opt/marachain

    COPY . ./

    #configure federation cli
    WORKDIR /opt/marachain/cliapp
    RUN npm install && npm run build && npm install -g . 


    #Configure sidechain node
    WORKDIR /opt/marachain
    RUN ./autogen.sh \
     && ./configure --without-gui \
     && make

    ENTRYPOINT ["./src/bitcoind"]

    CMD []
