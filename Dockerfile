# ============================================================================
# Coordinate Core - Multi-Stage Multi-Platform Build
# Builds native binaries for BOTH amd64 (Intel) AND arm64 (Apple Silicon)
# Expected size: ~100-150MB
# ============================================================================

# ------------------------------------------------------------------------------
# Stage 1: Builder
# ------------------------------------------------------------------------------
FROM debian:bookworm AS builder

ARG DEBIAN_FRONTEND=noninteractive
ARG TARGETPLATFORM
ARG TARGETARCH

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    bsdmainutils \
    python3 \
    libevent-dev \
    libboost-dev \
    libboost-system-dev \
    libboost-filesystem-dev \
    libboost-thread-dev \
    libsqlite3-dev \
    libminiupnpc-dev \
    libnatpmp-dev \
    libzmq3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# Copy source code
COPY . .

# Build with CMake
RUN echo "Building for ${TARGETPLATFORM} (${TARGETARCH})" && \
    cmake -B build \
        -DWITH_ZMQ=ON \
        -DBUILD_TESTS=OFF \
        -DBUILD_BENCH=OFF \
        -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build -j$(nproc) && \
    strip --strip-all build/bin/bitcoind || true && \
    strip --strip-all build/bin/bitcoin-cli || true

# ------------------------------------------------------------------------------
# Stage 2: Runtime (slim)
# ------------------------------------------------------------------------------
FROM debian:bookworm-slim

LABEL maintainer="dev@mara.tech"
LABEL version="0.2"
LABEL description="Coordinate Core - Multi-Platform Runtime Image"

# Install ALL required runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    # libevent - ALL components
    libevent-2.1-7 \
    libevent-core-2.1-7 \
    libevent-extra-2.1-7 \
    libevent-openssl-2.1-7 \
    libevent-pthreads-2.1-7 \
    # Boost libraries
    libboost-filesystem1.74.0 \
    libboost-thread1.74.0 \
    libboost-chrono1.74.0 \
    # Database
    libsqlite3-0 \
    # Networking
    libminiupnpc17 \
    libnatpmp1 \
    libzmq5 \
    # SSL/TLS (often needed)
    libssl3 \
    # Other common deps
    ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && apt-get clean

WORKDIR /opt/coordinate

# Copy ONLY binaries from builder stage (native for target platform)
COPY --from=builder --chmod=755 /src/build/bin/bitcoind ./bin/
COPY --from=builder --chmod=755 /src/build/bin/bitcoin-cli ./bin/

# Create data directory
RUN mkdir -p /root/.coordinate

ENTRYPOINT ["./bin/bitcoind"]
CMD []