FROM ubuntu:22.04

LABEL maintainer="dev@mara.tech"
LABEL version="0.1"
LABEL description="Docker file for coordinate"

ARG DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libevent-dev \
    libboost-dev \
    libsqlite3-dev \
    libminiupnpc-dev \
    libnatpmp-dev \
    libzmq3-dev \
    && rm -rf /var/lib/apt/lists/*

# Setup base directory
WORKDIR /opt/coordinate

# Copy build artifacts
COPY ./build ./

# Set permissions
RUN chmod +x ./bin/bitcoin-cli ./bin/bitcoind

ENTRYPOINT ["./bin/bitcoind"]
CMD []