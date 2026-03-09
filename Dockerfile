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

# Install build dependencies (including commonly missing ones for Bitcoin Core)
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    bsdmainutils \
    python3 \
    # Core crypto/SSL dependencies
    libssl-dev \
    # Event handling
    libevent-dev \
    # Boost libraries
    libboost-dev \
    libboost-system-dev \
    libboost-filesystem-dev \
    libboost-thread-dev \
    libboost-chrono-dev \
    libboost-program-options-dev \
    # Database
    libsqlite3-dev \
    # Networking
    libminiupnpc-dev \
    libnatpmp-dev \
    libzmq3-dev \
    # Additional tools that may be needed
    git \
    autoconf \
    automake \
    libtool \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# Copy source code
COPY . .

# Build with CMake - SEPARATE STEPS for better error visibility
# Step 1: Configure
RUN echo "=== Configuring for ${TARGETPLATFORM} (${TARGETARCH}) ===" && \
    cmake -B build \
        -DWITH_ZMQ=ON \
        -DBUILD_TESTS=OFF \
        -DBUILD_BENCH=OFF \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_VERBOSE_MAKEFILE=ON

# Step 2: Build (this is where failures typically occur)
RUN echo "=== Building ===" && \
    cmake --build build -j$(nproc)

# Step 3: Strip binaries (only if they exist)
RUN echo "=== Stripping binaries ===" && \
    if [ -f build/bin/bitcoind ]; then \
        strip --strip-all build/bin/bitcoind; \
    else \
        echo "ERROR: bitcoind binary not found!" && exit 1; \
    fi && \
    if [ -f build/bin/bitcoin-cli ]; then \
        strip --strip-all build/bin/bitcoin-cli; \
    else \
        echo "ERROR: bitcoin-cli binary not found!" && exit 1; \
    fi

# Verify binaries exist before proceeding
RUN ls -la build/bin/

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
    # SSL/TLS
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
