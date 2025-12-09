# P2TSH Wallet Documentation

Complete documentation for P2TSH (Pay-to-Taproot-Script-Hash) wallets on Coordinate blockchain.

## Overview

P2TSH is a witness version 2 address type supporting quantum-resistant cryptography through Schnorr and SLH-DSA signatures.

### Address Formats

| Network | Prefix | Example |
|---------|--------|---------|
| **Mainnet** | `cc1s` | `cc1sqav3hpwm2al3tvr6ywzsfs382fkg3ql9s0uwdvxrw2jh5...` |
| **Testnet** | `tc1s` | `tc1sqav3hpwm2al3tvr6ywzsfs382fkg3ql9s0uwdvxrw2jh5...` |
| **Regtest** | `ccrt1s` | `ccrt1sqav3hpwm2al3tvr6ywzsfs382fkg3ql9s0uwdvxrw2jh5...` |

### Signature Types

| Type | Size | Speed | Quantum-Resistant | Use Case |
|------|------|-------|-------------------|----------|
| **Schnorr** | 64 bytes | Fast | ❌ No | Development/Testing |
| **Hybrid** | ~7920 bytes | Moderate | ✅ Yes | **Production** (recommended) |
| **SLH-DSA** | 7856 bytes | Moderate | ✅ Yes | Maximum Security |

---

## 1. createwallet

Create a new P2TSH wallet.

### Syntax

```bash
./build/bin/bitcoin-cli -named createwallet \
  wallet_name="<name>" \
  descriptors=true \
  enable_p2tsh=true \
  p2tsh_signature_type="<schnorr|slh_dsa|hybrid>"
```

### Parameters

| Parameter | Required | Default | Values |
|-----------|----------|---------|--------|
| `wallet_name` | ✅ Yes | - | Wallet name string |
| `descriptors` | ✅ Yes | - | Must be `true` |
| `enable_p2tsh` | ✅ Yes | - | Must be `true` |
| `p2tsh_signature_type` | ❌ No | `"schnorr"` | `"schnorr"`, `"slh_dsa"`, `"hybrid"` |

### Examples

**Schnorr Wallet (Fast, Lightweight):**
```bash
./build/bin/bitcoin-cli -regtest -named createwallet \
  wallet_name="schnorr_wallet" \
  descriptors=true \
  enable_p2tsh=true \
  p2tsh_signature_type="schnorr"
```

**Hybrid Wallet (Quantum-Resistant) ⭐ Recommended:**
```bash
./build/bin/bitcoin-cli -regtest -named createwallet \
  wallet_name="hybrid_wallet" \
  descriptors=true \
  enable_p2tsh=true \
  p2tsh_signature_type="hybrid"
```

**SLH-DSA Wallet (Maximum Security):**
```bash
./build/bin/bitcoin-cli -regtest -named createwallet \
  wallet_name="slhdsa_wallet" \
  descriptors=true \
  enable_p2tsh=true \
  p2tsh_signature_type="slh_dsa"
```

### Response

```json
{
  "name": "schnorr_wallet",
  "warning": ""
}
```

---

## 2. loadwallet

Load an existing P2TSH wallet.

### Syntax

```bash
./build/bin/bitcoin-cli loadwallet "wallet_name"
```

### Example

```bash
./build/bin/bitcoin-cli -regtest loadwallet "schnorr_wallet"
```

### Response

```json
{
  "name": "schnorr_wallet",
  "warning": ""
}
```

### What Happens

1. Opens wallet database
2. Loads P2TSH metadata (merkle roots → keys)
3. Builds scriptPubKey cache via `GetScriptPubKeys()`
4. Wallet ready for transactions

### Debug Logs

```
[schnorr_wallet] P2TSH: GetScriptPubKeys() returning 5 scripts
[schnorr_wallet] Loaded P2TSH wallet with 5 keys
```

---

## 3. generatetoaddress

Mine blocks to a P2TSH address.

### Syntax

```bash
./build/bin/bitcoin-cli generatetoaddress <nblocks> "address"
```

### Parameters

| Parameter | Required | Description |
|-----------|----------|-------------|
| `nblocks` | ✅ Yes | Number of blocks to mine |
| `address` | ✅ Yes | P2TSH address (ccrt1s..., tc1s..., or cc1s...) |

### Example

```bash
# Get address
ADDR=$(./build/bin/bitcoin-cli -regtest -rpcwallet=schnorr_wallet \
  getnewaddress "" "p2tsh_schnorr" | jq -r '.address')

# Mine 101 blocks (100 for maturity + 1)
./build/bin/bitcoin-cli -regtest generatetoaddress 101 "$ADDR"
```

### Response

```json
[
  "00000000000001...",
  "00000000000002...",
  ...
  "000000000000101..."
]
```

### Check Balance

```bash
./build/bin/bitcoin-cli -regtest -rpcwallet=schnorr_wallet getbalance
```

**Response:** `5050.00000000` (101 × 50 CORD)

---

## 4. getaddressinfo

Get detailed information about a P2TSH address.

### Syntax

```bash
./build/bin/bitcoin-cli -rpcwallet=<wallet> getaddressinfo "address"
```

### Example

```bash
./build/bin/bitcoin-cli -regtest -rpcwallet=schnorr_wallet \
  getaddressinfo "ccrt1sqav3hpwm2al3tvr6ywzsfs382fkg3ql9s0uwdvxrw2jh500k3zvqc4h8a5"
```

### Response

```json
{
  "address": "ccrt1sqav3hpwm2al3tvr6ywzsfs382fkg3ql9s0uwdvxrw2jh500k3zvqc4h8a5",
  "scriptPubKey": "522055930a74d94ca3f66faa3dbdbce69a465b5122f9fe6e6de993702b1a3af4a409",
  "ismine": true,
  "solvable": true,
  "iswitness": true,
  "witness_version": 2,
  "witness_program": "55930a74d94ca3f66faa3dbdbce69a465b5122f9fe6e6de993702b1a3af4a409"
}
```

### Key Fields

| Field | Description |
|-------|-------------|
| `ismine` | Wallet can spend from this address |
| `witness_version` | Always `2` for P2TSH |
| `witness_program` | 32-byte merkle root (tapleaf hash) |
| `scriptPubKey` | Locking script: `OP_2 <32-byte merkle_root>` |

---

## 5. validateaddress

Validate P2TSH address format.

### Syntax

```bash
./build/bin/bitcoin-cli validateaddress "address"
```

### Example: Valid Address

```bash
./build/bin/bitcoin-cli -regtest validateaddress \
  "ccrt1sqav3hpwm2al3tvr6ywzsfs382fkg3ql9s0uwdvxrw2jh500k3zvqc4h8a5"
```

### Response

```json
{
  "isvalid": true,
  "address": "ccrt1sqav3hpwm2al3tvr6ywzsfs382fkg3ql9s0uwdvxrw2jh500k3zvqc4h8a5",
  "scriptPubKey": "522055930a74d94ca3f66faa3dbdbce69a465b5122f9fe6e6de993702b1a3af4a409",
  "iswitness": true,
  "witness_version": 2,
  "witness_program": "55930a74d94ca3f66faa3dbdbce69a465b5122f9fe6e6de993702b1a3af4a409"
}
```

### Example: Invalid Address

```bash
./build/bin/bitcoin-cli -regtest validateaddress "ccrt1sinvalidaddress"
```

### Response

```json
{
  "isvalid": false,
  "error_locations": [10, 15]
}
```

### Validation Rules

- ✅ Correct prefix (`ccrt1s`, `tc1s`, or `cc1s`)
- ✅ Valid Bech32m encoding
- ✅ Witness version = 2
- ✅ Program length = 32 bytes
- ✅ Valid checksum

---

## 6. sendtoaddress

Send funds from P2TSH address.

### Syntax

```bash
./build/bin/bitcoin-cli -rpcwallet=<wallet> sendtoaddress "address" <amount>
```

### Parameters

| Parameter | Required | Description |
|-----------|----------|-------------|
| `address` | ✅ Yes | Destination address (any type) |
| `amount` | ✅ Yes | Amount in CORD |

### Example: Schnorr Transaction

```bash
# Get destination
DEST=$(./build/bin/bitcoin-cli -regtest -rpcwallet=schnorr_wallet \
  getnewaddress "" "p2tsh_schnorr" | jq -r '.address')

# Send
./build/bin/bitcoin-cli -regtest -rpcwallet=schnorr_wallet sendtoaddress "$DEST" 0.1
```

**Response:** `a1b2c3d4e5f6...` (transaction ID)

### Transaction Structure

**Schnorr (64-byte signature):**
```
witness:
  [0]: 64-byte Schnorr signature
  [1]: 34-byte leaf script
  [2]: 1-byte control block (0xc1)
Total: ~100 bytes
```

**Hybrid (Schnorr + SLH-DSA):**
```
witness:
  [0]: 64-byte Schnorr signature
  [1]: 7857-byte SLH-DSA signature
  [2]: 70-byte leaf script
  [3]: 1-byte control block (0xc1)
Total: ~8000 bytes
```

### Check Transaction Size

```bash
TXID=$(./build/bin/bitcoin-cli -regtest -rpcwallet=hybrid_wallet sendtoaddress "$DEST" 0.5)

./build/bin/bitcoin-cli -regtest getrawtransaction "$TXID" true | \
  jq '{txid, size, vsize, weight}'
```

**Response:**
```json
{
  "txid": "a1b2c3d4...",
  "size": 8300,
  "vsize": 2100,
  "weight": 8400
}
```

### Debug Logs

```
P2TSH: SignTransaction called for 1 inputs
P2TSH: Checking input 0
P2TSH: Input 0 IS P2TSH
P2TSH: Found metadata, signature type: schnorr
P2TSH: Calculated sighash: ce3dc9bf...
P2TSH: Created Schnorr signature (64 bytes)
P2TSH: Successfully signed input 0
P2TSH: Signed 1 P2TSH inputs, all_signed=1
```

---

## Complete Test Examples

### Schnorr Workflow

```bash
CLI="./build/bin/bitcoin-cli -regtest -rpcuser=marachain -rpcpassword=marachain"

# 1. Create wallet
$CLI -named createwallet wallet_name="test" descriptors=true \
  enable_p2tsh=true p2tsh_signature_type="schnorr"

# 2. Generate address
ADDR1=$($CLI -rpcwallet=test getnewaddress "" "p2tsh_schnorr" | jq -r '.address')

# 3. Validate
$CLI validateaddress "$ADDR1"

# 4. Mine to address
$CLI generatetoaddress 101 "$ADDR1"

# 5. Check balance
$CLI -rpcwallet=test getbalance

# 6. Send transaction
ADDR2=$($CLI -rpcwallet=test getnewaddress "" "p2tsh_schnorr" | jq -r '.address')
$CLI -rpcwallet=test sendtoaddress "$ADDR2" 0.5

# 7. Mine block
$CLI generatetoaddress 1 "$ADDR1"

# 8. Send from new address
ADDR3=$($CLI -rpcwallet=test getnewaddress "" "p2tsh_schnorr" | jq -r '.address')
$CLI -rpcwallet=test sendtoaddress "$ADDR3" 0.1
```

### Hybrid Workflow

```bash
CLI="./build/bin/bitcoin-cli -regtest -rpcuser=marachain -rpcpassword=marachain"

# Create hybrid wallet
$CLI -named createwallet wallet_name="hybrid" descriptors=true \
  enable_p2tsh=true p2tsh_signature_type="hybrid"

# Generate and fund
ADDR=$($CLI -rpcwallet=hybrid getnewaddress "" "p2tsh_hybrid" | jq -r '.address')
$CLI generatetoaddress 101 "$ADDR"

# Send (creates ~8KB transaction)
DEST=$($CLI -rpcwallet=hybrid getnewaddress "" "p2tsh_hybrid" | jq -r '.address')
TXID=$($CLI -rpcwallet=hybrid sendtoaddress "$DEST" 1.0)

# Check transaction size
$CLI getrawtransaction "$TXID" true | jq '{size, vsize, weight}'
```

### Wallet Reload Test

```bash
CLI="./build/bin/bitcoin-cli -regtest -rpcuser=marachain -rpcpassword=marachain"

# Create and fund
$CLI -named createwallet wallet_name="reload" descriptors=true \
  enable_p2tsh=true p2tsh_signature_type="schnorr"

ADDR=$($CLI -rpcwallet=reload getnewaddress "" "p2tsh_schnorr" | jq -r '.address')
$CLI generatetoaddress 101 "$ADDR"

# Unload
$CLI unloadwallet "reload"

# Reload
$CLI loadwallet "reload"

# Verify balance persisted
$CLI -rpcwallet=reload getbalance

# Send transaction to verify keys work
DEST=$($CLI -rpcwallet=reload getnewaddress "" "p2tsh_schnorr" | jq -r '.address')
$CLI -rpcwallet=reload sendtoaddress "$DEST" 0.1
```

---

## Comparison Table

| Feature | Schnorr | SLH-DSA | Hybrid |
|---------|---------|---------|--------|
| **Signature Size** | 64 bytes | 7856 bytes | 7920 bytes |
| **TX Size** | ~200 B | ~8 KB | ~8 KB |
| **Speed** | <1ms | ~50ms | ~50ms |
| **Quantum-Resistant** | ❌ No | ✅ Yes | ✅ Yes |
| **Classical Security** | ✅ Yes | ❌ No | ✅ Yes |
| **Fees** | Low | High | High |
| **Use Case** | Testing | Max Security | **Production** |

### Recommendations

- 🧪 **Development**: Use `schnorr` (fast, lightweight)
- 🏭 **Production**: Use `hybrid` (quantum-resistant)
- 🔒 **Maximum Security**: Use `slh_dsa` (pure post-quantum)

---

## Troubleshooting

### Empty Witness After Mining

**Symptoms:**
- First transaction works
- After mining, second transaction fails with empty witness

**Fix:** Implement `GetScriptPubKeys()` method

### Wallet Won't Load

**Error:** `Unexpected legacy entry found in descriptor wallet`

**Fix:**
```bash
rm -rf ~/.coordinate/regtest/wallets/wallet_name
# Recreate wallet with current code
```

### Invalid Schnorr Signature

**Cause:**
- Wrong tapleaf hash (using 0xc1 instead of 0xc0)
- Missing spent outputs

**Fix:**
- Use TAPROOT_LEAF_TAPSCRIPT (0xc0) for tapleaf hash
- Use P2TSH_LEAF_TAPSCRIPT (0xc1) for control block

### View Debug Logs

```bash
tail -f ~/.coordinate/regtest/debug.log | grep "P2TSH:"
```

---

## Additional Commands

### List Wallets
```bash
./build/bin/bitcoin-cli -regtest listwallets
```

### Get Wallet Info
```bash
./build/bin/bitcoin-cli -regtest -rpcwallet=schnorr_wallet getwalletinfo
```

### List Unspent
```bash
./build/bin/bitcoin-cli -regtest -rpcwallet=schnorr_wallet listunspent
```

### Get Transaction
```bash
./build/bin/bitcoin-cli -regtest getrawtransaction "txid" true
```

### Unload Wallet
```bash
./build/bin/bitcoin-cli -regtest unloadwallet "wallet_name"
```

---

## Summary

P2TSH provides quantum-ready addressing with three signature types:

- **Schnorr**: Fast, lightweight (testing)
- **Hybrid**: Quantum-resistant (production) ⭐
- **SLH-DSA**: Maximum security

**Network Prefixes:**
- Regtest: `ccrt1s`
- Testnet: `tc1s`
- Mainnet: `cc1s`

**Requirements:**
- Descriptor wallets (`descriptors=true`)
- P2TSH enabled (`enable_p2tsh=true`)
- Witness version 2

---

**Document Version:** 1.0  
**Last Updated:** 2025-12-09  
**Network:** Coordinate Blockchain