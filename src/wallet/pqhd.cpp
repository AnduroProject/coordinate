// Copyright (c) 2024-2026 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/pqhd.h>
#include <crypto/hmac_sha512.h>
#include <hash.h>
#include <logging.h>
#include <support/cleanse.h>
#include <util/strencodings.h>

#include "../secp256k1/include/secp256k1.h"
#include "../secp256k1/include/secp256k1_extrakeys.h"

extern "C" {
#include <libbitcoinpqc/slh_dsa.h>
}

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <cstring>

namespace wallet {

// ============================================================================
// PQHDPath
// ============================================================================

std::string PQHDPath::ToString() const
{
    std::ostringstream ss;
    ss << "m/" << purpose << "'/" << coin_type << "'/" << account << "'/"
       << algorithm << "'/" << change << "'/" << index;
    return ss.str();
}

std::vector<uint32_t> PQHDPath::ToVector() const
{
    return {
        purpose   | PQHD_HARDENED,
        coin_type | PQHD_HARDENED,
        account   | PQHD_HARDENED,
        algorithm | PQHD_HARDENED,
        change    | PQHD_HARDENED,
        index     // leaf index — NOT hardened per BIP-360 test vectors
    };
}

std::optional<PQHDPath> PQHDPath::Parse(const std::string& path_str)
{
    // Expected: "m/360'/0'/0'/340'/0'/0" or "m/360'/0'/0'/205'/0'/0"
    PQHDPath path;

    std::string s = path_str;
    if (s.empty()) return std::nullopt;

    // Strip leading "m/"
    if (s.substr(0, 2) == "m/") {
        s = s.substr(2);
    } else {
        return std::nullopt;
    }

    // Split by '/'
    std::vector<std::string> parts;
    std::istringstream iss(s);
    std::string token;
    while (std::getline(iss, token, '/')) {
        parts.push_back(token);
    }

    if (parts.size() != 6) return std::nullopt;

    auto parse_component = [](const std::string& comp, uint32_t& out, bool& hardened) -> bool {
        hardened = false;
        std::string num_str = comp;
        if (!num_str.empty() && (num_str.back() == '\'' || num_str.back() == 'h')) {
            hardened = true;
            num_str.pop_back();
        }
        try {
            unsigned long val = std::stoul(num_str);
            if (val > 0x7FFFFFFF) return false;
            out = static_cast<uint32_t>(val);
            return true;
        } catch (...) {
            return false;
        }
    };

    bool h;
    if (!parse_component(parts[0], path.purpose, h) || path.purpose != PQHD_PURPOSE) return std::nullopt;
    if (!parse_component(parts[1], path.coin_type, h)) return std::nullopt;
    if (!parse_component(parts[2], path.account, h)) return std::nullopt;
    if (!parse_component(parts[3], path.algorithm, h)) return std::nullopt;
    if (!parse_component(parts[4], path.change, h)) return std::nullopt;
    if (!parse_component(parts[5], path.index, h)) return std::nullopt;

    // Validate algorithm type
    if (path.algorithm != PQHD_ALGO_SCHNORR && path.algorithm != PQHD_ALGO_SLH_DSA) {
        return std::nullopt;
    }

    return path;
}

// ============================================================================
// PQHDKeyDerivation
// ============================================================================

PQHDKeyDerivation::PQHDKeyDerivation() = default;
PQHDKeyDerivation::~PQHDKeyDerivation() = default;

bool PQHDKeyDerivation::SetSeed(const std::vector<unsigned char>& bip39_seed)
{
    if (bip39_seed.size() != 64) {
        LogPrintf("PQHD: Invalid BIP39 seed size: %zu (expected 64)\n", bip39_seed.size());
        return false;
    }

    // Master derivation: HMAC-SHA512("Bitcoin seed", bip39_seed) → [seed(32) | cc(32)]
    static const unsigned char hmac_key[] = {'B','i','t','c','o','i','n',' ','s','e','e','d'};
    unsigned char output[64];
    CHMAC_SHA512(hmac_key, sizeof(hmac_key))
        .Write(bip39_seed.data(), bip39_seed.size())
        .Finalize(output);

    m_master.seed.assign(output, output + 32);
    std::memcpy(m_master.chaincode.begin(), output + 32, 32);
    m_master.depth = 0;
    m_master.child_index = 0;
    std::memset(m_master.fingerprint, 0, 4);

    memory_cleanse(output, sizeof(output));

    m_seeded = true;
    LogPrintf("PQHD: Master node initialized from %zu-byte seed\n", bip39_seed.size());
    return true;
}

bool PQHDKeyDerivation::DeriveChild(
    const PQHDNode& parent,
    uint32_t child_index,
    PQHDNode& child) const
{
    if (!parent.IsValid()) return false;

    // PQHD uses the same HMAC construction as BIP32 hardened derivation,
    // but WITHOUT the secp256k1 key addition step.
    //
    // HMAC-SHA512(parent.chaincode, 0x00 || parent.seed(32) || index_BE(4)) → [IL(32) | IR(32)]
    // child.seed = IL        (NOT IL + parent_key as in BIP32)
    // child.chaincode = IR

    unsigned char output[64];
    BIP32Hash(parent.chaincode, child_index, 0, parent.seed.data(), output);

    child.seed.assign(output, output + 32);
    std::memcpy(child.chaincode.begin(), output + 32, 32);
    child.depth = parent.depth + 1;
    child.child_index = child_index;

    // Fingerprint = first 4 bytes of Hash160(parent.seed)
    uint160 parent_id = Hash160(parent.seed);
    std::memcpy(child.fingerprint, parent_id.begin(), 4);

    memory_cleanse(output, sizeof(output));
    return true;
}

bool PQHDKeyDerivation::DerivePath(const PQHDPath& path, PQHDNode& node) const
{
    if (!m_seeded) {
        LogPrintf("PQHD: Cannot derive - engine not seeded\n");
        return false;
    }

    std::vector<uint32_t> indices = path.ToVector();

    PQHDNode current = m_master;
    for (uint32_t idx : indices) {
        PQHDNode next;
        if (!DeriveChild(current, idx, next)) {
            LogPrintf("PQHD: Child derivation failed at index %u\n", idx);
            return false;
        }
        current = next;
    }

    node = current;
    return true;
}

// ============================================================================
// Algorithm-specific key generation
// ============================================================================

bool PQHDKeyDerivation::GenerateSchnorrKeyPair(
    const std::vector<unsigned char>& seed,
    std::vector<unsigned char>& secret_key,
    std::vector<unsigned char>& public_key) const
{
    if (seed.size() != 32) return false;

    // The derived seed IS the secp256k1 secret key
    secret_key = seed;

    // Derive x-only public key
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    if (!ctx) return false;

    secp256k1_keypair keypair;
    if (!secp256k1_keypair_create(ctx, &keypair, seed.data())) {
        secp256k1_context_destroy(ctx);
        LogPrintf("PQHD: secp256k1 keypair creation failed (invalid key)\n");
        return false;
    }

    secp256k1_xonly_pubkey xonly;
    secp256k1_keypair_xonly_pub(ctx, &xonly, nullptr, &keypair);

    public_key.resize(32);
    secp256k1_xonly_pubkey_serialize(ctx, public_key.data(), &xonly);

    secp256k1_context_destroy(ctx);
    return true;
}

bool PQHDKeyDerivation::GenerateSLHDSAKeyPair(
    const std::vector<unsigned char>& seed,
    std::vector<unsigned char>& secret_key,
    std::vector<unsigned char>& public_key) const
{
    if (seed.size() != 32) return false;

    secret_key.resize(SLH_DSA_SHAKE_128S_SECRET_KEY_SIZE);
    public_key.resize(SLH_DSA_SHAKE_128S_PUBLIC_KEY_SIZE);

    // SLH-DSA-SHAKE-128s keygen requires >= 3*n = 48 bytes of entropy.
    // Expand our 32-byte derived seed to 128 bytes deterministically
    // using two rounds of HMAC-SHA512.
    std::vector<unsigned char> expanded(128, 0);
    {
        std::vector<unsigned char> tag_seed{'S','L','H','D','S','A','_','S','E','E','D','_','0'};
        CHMAC_SHA512 hmac0(seed.data(), seed.size());
        hmac0.Write(tag_seed.data(), tag_seed.size());
        unsigned char buf0[64];
        hmac0.Finalize(buf0);
        std::copy(buf0, buf0 + 64, expanded.begin());

        tag_seed.back() = '1';
        CHMAC_SHA512 hmac1(seed.data(), seed.size());
        hmac1.Write(tag_seed.data(), tag_seed.size());
        unsigned char buf1[64];
        hmac1.Finalize(buf1);
        std::copy(buf1, buf1 + 64, expanded.begin() + 64);
    }

    int result = slh_dsa_shake_128s_keygen(
        public_key.data(),
        secret_key.data(),
        expanded.data(),
        expanded.size());

    if (result != 0) {
        LogPrintf("PQHD: SLH-DSA keygen failed with code %d\n", result);
        return false;
    }

    return true;
}

bool PQHDKeyDerivation::GenerateKeyPair(
    const PQHDNode& node,
    uint32_t algorithm,
    PQHDKeyPair& keypair) const
{
    if (!node.IsValid()) return false;

    keypair.algorithm = algorithm;
    keypair.node = node;

    switch (algorithm) {
    case PQHD_ALGO_SCHNORR:
        return GenerateSchnorrKeyPair(node.seed, keypair.secret_key, keypair.public_key);

    case PQHD_ALGO_SLH_DSA:
        return GenerateSLHDSAKeyPair(node.seed, keypair.secret_key, keypair.public_key);

    default:
        LogPrintf("PQHD: Unknown algorithm type %u\n", algorithm);
        return false;
    }
}

bool PQHDKeyDerivation::DeriveKeyPair(const PQHDPath& path, PQHDKeyPair& keypair) const
{
    PQHDNode node;
    if (!DerivePath(path, node)) return false;
    return GenerateKeyPair(node, path.algorithm, keypair);
}

bool PQHDKeyDerivation::DeriveP2MRKeyPairs(
    uint32_t coin_type,
    uint32_t account,
    uint32_t change,
    uint32_t index,
    PQHDKeyPair& schnorr_pair,
    PQHDKeyPair& slhdsa_pair) const
{
    // Schnorr: m/360'/coin'/account'/340'/change'/index
    PQHDPath schnorr_path;
    schnorr_path.purpose = PQHD_PURPOSE;
    schnorr_path.coin_type = coin_type;
    schnorr_path.account = account;
    schnorr_path.algorithm = PQHD_ALGO_SCHNORR;
    schnorr_path.change = change;
    schnorr_path.index = index;

    if (!DeriveKeyPair(schnorr_path, schnorr_pair)) {
        LogPrintf("PQHD: Schnorr derivation failed for %s\n", schnorr_path.ToString());
        return false;
    }

    // SLH-DSA: m/360'/coin'/account'/205'/change'/index
    PQHDPath slhdsa_path;
    slhdsa_path.purpose = PQHD_PURPOSE;
    slhdsa_path.coin_type = coin_type;
    slhdsa_path.account = account;
    slhdsa_path.algorithm = PQHD_ALGO_SLH_DSA;
    slhdsa_path.change = change;
    slhdsa_path.index = index;

    if (!DeriveKeyPair(slhdsa_path, slhdsa_pair)) {
        LogPrintf("PQHD: SLH-DSA derivation failed for %s\n", slhdsa_path.ToString());
        return false;
    }

    LogPrintf("PQHD: Derived P2MR keypairs at index %u (schnorr: %s, slh-dsa: %s)\n",
              index,
              HexStr(schnorr_pair.public_key).substr(0, 16) + "...",
              HexStr(slhdsa_pair.public_key).substr(0, 16) + "...");

    return true;
}

} // namespace wallet