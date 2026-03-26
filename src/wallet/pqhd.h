// Copyright (c) 2024-2026 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_PQHD_H
#define BITCOIN_WALLET_PQHD_H

#include <hash.h>
#include <uint256.h>
#include <serialize.h>

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

/**
 * Post-Quantum Hierarchical Deterministic Key Derivation (PQHD)
 *
 * Implements the BIP-360 PQHD key derivation scheme. Unlike standard BIP-32,
 * PQHD uses pure HMAC-SHA512 chaining without elliptic curve operations,
 * making it algorithm-agnostic. The derived seed is then used for
 * algorithm-specific key generation at the leaf level.
 *
 * Derivation path:
 *   m / 360' / coin_type' / account' / algorithm' / change' / index
 *
 * Algorithm types (purpose sub-paths):
 *   340' = BIP-340 Schnorr (secp256k1 x-only pubkey)
 *   205' = SLH-DSA-SHAKE-128s (SPHINCS+ post-quantum)
 *
 * Master derivation:
 *   HMAC-SHA512("Bitcoin seed", bip39_seed) → [master_seed(32) | master_cc(32)]
 *
 * Child derivation (hardened only):
 *   HMAC-SHA512(parent_cc, 0x00 || parent_seed(32) || index_BE(4)) → [child_seed(32) | child_cc(32)]
 *
 * Final key generation:
 *   Schnorr:  child_seed → secp256k1 private key → x-only pubkey
 *   SLH-DSA:  child_seed → slh_dsa_shake_128s_keygen(entropy=child_seed) → SK(64) + PK(32)
 */

namespace wallet {

/** BIP-360 purpose level */
static constexpr uint32_t PQHD_PURPOSE = 360;

/** Algorithm type constants (used at depth 3 in the derivation path) */
static constexpr uint32_t PQHD_ALGO_SCHNORR = 340;   //!< BIP-340 Schnorr
static constexpr uint32_t PQHD_ALGO_SLH_DSA = 205;   //!< SLH-DSA-SHAKE-128s

/** Hardened derivation flag */
static constexpr uint32_t PQHD_HARDENED = 0x80000000;

/** Change indices */
static constexpr uint32_t PQHD_CHANGE_EXTERNAL = 0;  //!< Receiving addresses
static constexpr uint32_t PQHD_CHANGE_INTERNAL = 1;   //!< Change addresses

/**
 * Result of a PQHD key derivation step.
 * Contains the derived 32-byte seed and 32-byte chain code.
 */
struct PQHDNode {
    std::vector<unsigned char> seed;  //!< 32-byte derived seed
    ChainCode chaincode;              //!< 32-byte chain code for child derivation
    unsigned char depth{0};           //!< Current derivation depth
    uint32_t child_index{0};          //!< Child index at this level
    unsigned char fingerprint[4]{};   //!< Parent fingerprint (first 4 bytes of Hash160(parent_seed))

    PQHDNode() : seed(32, 0) {}

    /** Check if this node contains valid data */
    bool IsValid() const { return seed.size() == 32; }
};

/**
 * Result of algorithm-specific key generation from a derived seed.
 */
struct PQHDKeyPair {
    std::vector<unsigned char> secret_key;   //!< Algorithm-specific secret key
    std::vector<unsigned char> public_key;    //!< Algorithm-specific public key
    uint32_t algorithm{0};                    //!< Algorithm type (340 or 205)
    PQHDNode node;                            //!< The derivation node that produced this key

    bool IsValid() const { return !secret_key.empty() && !public_key.empty(); }
};

/**
 * Parsed derivation path.
 */
struct PQHDPath {
    uint32_t purpose{PQHD_PURPOSE};
    uint32_t coin_type{0};
    uint32_t account{0};
    uint32_t algorithm{0};     //!< 340 or 205
    uint32_t change{0};        //!< 0=external, 1=internal
    uint32_t index{0};         //!< Address index

    /** Convert to string form: m/360'/0'/0'/340'/0'/0 */
    std::string ToString() const;

    /** Parse from string: "m/360'/0'/0'/340'/0'/0" */
    static std::optional<PQHDPath> Parse(const std::string& path_str);

    /** Get the full path as a vector of hardened indices */
    std::vector<uint32_t> ToVector() const;
};

class PQHDKeyDerivation {
public:
    PQHDKeyDerivation();
    ~PQHDKeyDerivation();

    /**
     * Initialize from a BIP39 mnemonic seed (64 bytes from PBKDF2).
     * Computes master seed and chain code via HMAC-SHA512("Bitcoin seed", seed).
     *
     * @param[in] bip39_seed  The 64-byte seed from BIP39 mnemonic + passphrase
     * @returns true on success
     */
    bool SetSeed(const std::vector<unsigned char>& bip39_seed);

    /**
     * Derive a child node at the given hardened index from a parent node.
     * Uses pure HMAC-SHA512 chaining (no elliptic curve operations).
     *
     * HMAC-SHA512(parent.chaincode, 0x00 || parent.seed || index_BE) → [child_seed | child_cc]
     *
     * @param[in] parent      Parent node
     * @param[in] child_index Index (will be OR'd with PQHD_HARDENED automatically)
     * @param[out] child      Resulting child node
     * @returns true on success
     */
    bool DeriveChild(const PQHDNode& parent, uint32_t child_index, PQHDNode& child) const;

    /**
     * Derive through a complete PQHD path from the master node.
     *
     * @param[in] path  The full derivation path
     * @param[out] node The resulting node at the end of the path
     * @returns true on success
     */
    bool DerivePath(const PQHDPath& path, PQHDNode& node) const;

    /**
     * Generate an algorithm-specific keypair from a derived node.
     *
     * For Schnorr (340): seed → secp256k1 private key → x-only pubkey
     * For SLH-DSA (205): seed → slh_dsa_shake_128s_keygen → SK(64) + PK(32)
     *
     * @param[in] node       The derived node containing the seed
     * @param[in] algorithm  Algorithm type (PQHD_ALGO_SCHNORR or PQHD_ALGO_SLH_DSA)
     * @param[out] keypair   The resulting algorithm-specific keypair
     * @returns true on success
     */
    bool GenerateKeyPair(const PQHDNode& node, uint32_t algorithm, PQHDKeyPair& keypair) const;

    /**
     * Convenience: derive full path and generate keypair in one call.
     *
     * @param[in] path     Full derivation path (algorithm is inferred from path)
     * @param[out] keypair The resulting keypair
     * @returns true on success
     */
    bool DeriveKeyPair(const PQHDPath& path, PQHDKeyPair& keypair) const;

    /**
     * Derive both Schnorr and SLH-DSA keypairs for a given account/change/index.
     * This is the primary method for P2MR address generation.
     *
     * Derives:
     *   m/360'/coin'/account'/340'/change'/index → Schnorr keypair
     *   m/360'/coin'/account'/205'/change'/index → SLH-DSA keypair
     *
     * @param[in] coin_type     Coin type (0 for Bitcoin mainnet)
     * @param[in] account       Account index
     * @param[in] change        0=external, 1=internal
     * @param[in] index         Address index
     * @param[out] schnorr_pair Schnorr keypair result
     * @param[out] slhdsa_pair  SLH-DSA keypair result
     * @returns true if both derivations succeed
     */
    bool DeriveP2MRKeyPairs(uint32_t coin_type, uint32_t account,
                            uint32_t change, uint32_t index,
                            PQHDKeyPair& schnorr_pair,
                            PQHDKeyPair& slhdsa_pair) const;

    /** Get the master node (for testing/debugging) */
    const PQHDNode& GetMasterNode() const { return m_master; }

    /** Check if the derivation engine is seeded */
    bool IsSeeded() const { return m_seeded; }

private:
    PQHDNode m_master;     //!< Master node derived from BIP39 seed
    bool m_seeded{false};  //!< Whether SetSeed has been called

    /** Generate Schnorr keypair from seed */
    bool GenerateSchnorrKeyPair(const std::vector<unsigned char>& seed,
                                std::vector<unsigned char>& secret_key,
                                std::vector<unsigned char>& public_key) const;

    /** Generate SLH-DSA keypair from seed */
    bool GenerateSLHDSAKeyPair(const std::vector<unsigned char>& seed,
                               std::vector<unsigned char>& secret_key,
                               std::vector<unsigned char>& public_key) const;
};

} // namespace wallet

#endif // BITCOIN_WALLET_PQHD_H
