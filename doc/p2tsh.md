# Coordinate BIP-360 Post-Quantum Wallet Guide

Guide for using Coordinate's BIP-360 post-quantum cryptographic wallets.

## Overview

BIP-360 adds quantum-resistant signatures to Coordinate with three signature schemes:

- **Hybrid**: Traditional + Quantum-resistant (Recommended)
- **Schnorr**: Traditional signatures only
- **SLHDSA**: Fully quantum-resistant

## Prerequisites

```conf
# bitcoin.conf
regtest=1
rpcuser=marachain
rpcpassword=marachain
rpcport=18443
```

## 1. Create Wallets

**Hybrid Wallet:**
```bash
./build/bin/bitcoin-cli -rpcuser=marachain -rpcpassword=marachain \
  -rpcport=18443 -regtest=1 \
  createwallet "test_p2tsh" false false "" false true false false true "hybrid"
```

**Schnorr Wallet:**
```bash
./build/bin/bitcoin-cli -rpcuser=marachain -rpcpassword=marachain \
  -rpcport=18443 -regtest=1 \
  createwallet "schnorr_wallet" false false "" false true false false true "schnorr"
```

**SLHDSA Wallet:**
```bash
./build/bin/bitcoin-cli -rpcuser=marachain -rpcpassword=marachain \
  -rpcport=18443 -regtest=1 \
  createwallet "slhdsa_wallet" false false "" false true false false true "slhdsa"
```

## 2. Generate Addresses

**Hybrid Address:**
```bash
./build/bin/bitcoin-cli -rpcuser=marachain -rpcpassword=marachain \
  -rpcport=18443 -regtest=1 \
  -rpcwallet=test_p2tsh getnewaddress "" "p2tsh_hybrid"
```

**Schnorr Address:**
```bash
./build/bin/bitcoin-cli -rpcuser=marachain -rpcpassword=marachain \
  -rpcport=18443 -regtest=1 \
  -rpcwallet=schnorr_wallet getnewaddress "" "p2tsh_schnorr"
```

**SLHDSA Address:**
```bash
./build/bin/bitcoin-cli -rpcuser=marachain -rpcpassword=marachain \
  -rpcport=18443 -regtest=1 \
  -rpcwallet=slhdsa_wallet getnewaddress "" "p2tsh_slhdsa"
```

## 3. Address Operations

**List Received Addresses:**
```bash
./build/bin/bitcoin-cli -rpcuser=marachain -rpcpassword=marachain \
  -rpcport=18443 -regtest=1 \
  -rpcwallet=test_p2tsh listreceivedbyaddress 0 true | jq -r '.[].address'
```

**Get Address Info:**
```bash
./build/bin/bitcoin-cli -rpcuser=marachain -rpcpassword=marachain \
  -rpcport=18443 -regtest=1 \
  getaddressinfo ccrt1zgrunzrm9jg2uf9ge2ydrshap7v43z060tmh6sg2u4t4m83ugsa6sgrdv85
```

**Validate Address:**
```bash
./build/bin/bitcoin-cli -rpcuser=marachain -rpcpassword=marachain \
  -rpcport=18443 -regtest=1 \
  validateaddress ccrt1zgrunzrm9jg2uf9ge2ydrshap7v43z060tmh6sg2u4t4m83ugsa6sgrdv85
```

## 4. Mining & Transactions

**Generate Blocks:**
```bash
./build/bin/bitcoin-cli -rpcuser=marachain -rpcpassword=marachain \
  -rpcport=18443 -regtest=1 \
  generatetoaddress 1000 ccrt1zqrgy7n48gw5vhhdpfph3m2y6jgnl6auvwjcdvsvychx769zk0g5q2wy994
```

**Send Transaction:**
```bash
./build/bin/bitcoin-cli -rpcuser=marachain -rpcpassword=marachain \
  -rpcport=18443 -regtest=1 \
  sendtoaddress ccrt1zwr6kkcwe2nqrt60rnpyzgs25wus3wynngjzl4sj6x6jq0cnm0q6sm434fg 0.1
```
