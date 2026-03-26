// Copyright (c) 2024-2026 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_P2MR_SCRIPTPUBKEYMAN_H
#define BITCOIN_WALLET_P2MR_SCRIPTPUBKEYMAN_H

#include <wallet/scriptpubkeyman.h>
#include <wallet/pqhd.h>
#include <addresstype.h>
#include <script/descriptor.h>
#include <uint256.h>
#include <pubkey.h>
#include <serialize.h>

#include <map>
#include <vector>
#include <optional>

struct secp256k1_context_struct;
typedef struct secp256k1_context_struct secp256k1_context;

namespace wallet {

/**
 * P2MR spend path selection.
 *
 * Every P2MR address has a 3-leaf Taptree:
 *
 *              merkle_root
 *             /            \
 *          branch          leaf3 (HYBRID)
 *         /      \
 *     leaf1       leaf2
 *   (SCHNORR)   (SLH_DSA)
 *
 * The spender picks which leaf to reveal at spend time.
 */
enum class P2MRSpendType : uint8_t {
    SCHNORR  = 0,  //!< Leaf 1: Schnorr-only (smallest witness, pre-quantum)
    SLH_DSA  = 1,  //!< Leaf 2: SLH-DSA-only (post-quantum, ~7.9 KB signature)
    HYBRID   = 2,  //!< Leaf 3: Schnorr + SLH-DSA (maximum security, both required)
};

/** Number of leaves in a P2MR address tree. */
static constexpr size_t P2MR_NUM_LEAVES = 3;

/**
 * Per-leaf data stored in P2MRKeyMetadata.
 */
struct P2MRLeafInfo {
    CScript script;                          //!< Leaf script
    std::vector<unsigned char> control_block; //!< P2MR control block (0xc1 + merkle path)
    uint256 leaf_hash;                        //!< TapLeaf hash of this leaf

    SERIALIZE_METHODS(P2MRLeafInfo, obj) {
        READWRITE(obj.script, obj.control_block, obj.leaf_hash);
    }
};

/**
 * Metadata for a single P2MR address.
 *
 * Every address always has both key types and all 3 leaf scripts.
 * The merkle root commits to the entire 3-leaf tree.
 */
struct P2MRKeyMetadata {
    static constexpr uint8_t CURRENT_VERSION = 2;  //!< v2 = 3-leaf tree

    uint8_t version{CURRENT_VERSION};
    std::vector<unsigned char> schnorr_pubkey;  //!< 32-byte x-only Schnorr pubkey
    std::vector<unsigned char> slh_dsa_pubkey;  //!< SLH-DSA-SHAKE-128s pubkey
    uint256 merkle_root;                        //!< Root committed in OP_2 <root>
    P2MRLeafInfo leaves[P2MR_NUM_LEAVES];       //!< Indexed by P2MRSpendType
    int64_t create_time{0};

    SERIALIZE_METHODS(P2MRKeyMetadata, obj) {
        READWRITE(obj.version);
        READWRITE(obj.schnorr_pubkey, obj.slh_dsa_pubkey);
        READWRITE(obj.merkle_root);
        for (size_t i = 0; i < P2MR_NUM_LEAVES; i++) {
            READWRITE(obj.leaves[i]);
        }
        READWRITE(obj.create_time);
    }

    /** Get leaf info for a given spend type. */
    const P2MRLeafInfo& GetLeaf(P2MRSpendType type) const {
        return leaves[static_cast<uint8_t>(type)];
    }
};

class P2MRScriptPubKeyMan : public ScriptPubKeyMan
{
private:
    mutable RecursiveMutex cs_p2mr;

    //! Schnorr private keys indexed by Hash160(pubkey)
    std::map<CKeyID, std::vector<unsigned char>> mapSchnorrKeys GUARDED_BY(cs_p2mr);
    //! SLH-DSA private keys indexed by Hash160(pubkey)
    std::map<CKeyID, std::vector<unsigned char>> mapSLHDSAKeys GUARDED_BY(cs_p2mr);
    //! Encrypted key storage (for wallet encryption support)
    std::map<CKeyID, std::vector<unsigned char>> mapCryptedSchnorrKeys GUARDED_BY(cs_p2mr);
    std::map<CKeyID, std::vector<unsigned char>> mapCryptedSLHDSAKeys GUARDED_BY(cs_p2mr);
    //! All P2MR address metadata, indexed by merkle root
    std::map<uint256, P2MRKeyMetadata> mapP2MRMetadata GUARDED_BY(cs_p2mr);

    //! Preferred spend type (which leaf to use when signing)
    P2MRSpendType m_preferred_spend_type{P2MRSpendType::SCHNORR};

    secp256k1_context* secp_ctx{nullptr};

    // ---- PQHD Derivation ----
    std::unique_ptr<PQHDKeyDerivation> m_pqhd GUARDED_BY(cs_p2mr);
    uint32_t m_coin_type GUARDED_BY(cs_p2mr) = 0;     //!< BIP-360 coin type (0=mainnet)
    uint32_t m_account GUARDED_BY(cs_p2mr) = 0;        //!< Account index
    uint32_t m_next_external_index GUARDED_BY(cs_p2mr) = 0; //!< Next external (receive) index
    uint32_t m_next_internal_index GUARDED_BY(cs_p2mr) = 0; //!< Next internal (change) index

    // ---- Key generation ----
    bool GenerateSchnorrKey(std::vector<unsigned char>& priv,
                            std::vector<unsigned char>& pub);
    bool GenerateSLHDSAKey(std::vector<unsigned char>& priv,
                           std::vector<unsigned char>& pub);

    // ---- Leaf script construction ----
    static CScript BuildSchnorrLeafScript(const std::vector<unsigned char>& schnorr_pk);
    static CScript BuildSLHDSALeafScript(const std::vector<unsigned char>& slh_dsa_pk);
    static CScript BuildHybridLeafScript(const std::vector<unsigned char>& schnorr_pk,
                                         const std::vector<unsigned char>& slh_dsa_pk);

    // ---- Tree construction ----
    bool BuildP2MRTree(const std::vector<unsigned char>& schnorr_pk,
                       const std::vector<unsigned char>& slh_dsa_pk,
                       P2MRKeyMetadata& metadata) const;

    // ---- Signing helpers ----
    bool SignSchnorr(const std::vector<unsigned char>& privkey,
                     const uint256& sighash,
                     std::vector<unsigned char>& signature) const;

    bool SignSLHDSA(const std::vector<unsigned char>& privkey,
                    const uint256& sighash,
                    std::vector<unsigned char>& signature) const;

public:
    explicit P2MRScriptPubKeyMan(WalletStorage& storage);
    virtual ~P2MRScriptPubKeyMan();

    // ---- Address generation ----
    util::Result<CTxDestination> GetNewDestination(const OutputType type) override;
    util::Result<CTxDestination> GetNewP2MRDestination();

    // ---- PQHD seed-based derivation ----
    /**
     * Initialize from a BIP39 seed for deterministic derivation.
     * After calling this, GetNewP2MRDestination() uses PQHD paths
     * instead of random key generation.
     *
     * @param[in] bip39_seed  64-byte seed from BIP39 mnemonic
     * @param[in] coin_type   Coin type (0 for Bitcoin mainnet)
     * @param[in] account     Account index
     * @returns true on success
     */
    bool SetSeed(const std::vector<unsigned char>& bip39_seed,
                 uint32_t coin_type = 0, uint32_t account = 0);

    /** Check if PQHD derivation is active (vs random key generation). */
    bool IsHDEnabled() const override;

    //! Get the p2mr() descriptor string for this wallet.
    //! Returns something like:
    //!   p2mr(xpub.../360h/0h/0h/340h/0h/<idx>, xpub.../360h/0h/0h/205h/0h/<idx>)
    //! or for a specific address:
    //!   p2mr(schnorr_pubkey, slhdsa_pubkey)
    std::string GetDescriptorString(const uint256& merkle_root) const;

    /** Get next unused derivation index for receive (external) addresses. */
    uint32_t GetNextExternalIndex() const EXCLUSIVE_LOCKS_REQUIRED(cs_p2mr) { return m_next_external_index; }
    /** Get next unused derivation index for change (internal) addresses. */
    uint32_t GetNextInternalIndex() const EXCLUSIVE_LOCKS_REQUIRED(cs_p2mr) { return m_next_internal_index; }

    // ---- Standard wallet interface (same as DescriptorScriptPubKeyMan) ----
    bool CanGetAddresses(bool internal = false) const override { return true; }
    bool CanProvide(const CScript& script, SignatureData& sigdata) override;
    util::Result<CTxDestination> GetReservedDestination(const OutputType type, bool internal, int64_t& index) override;
    void ReturnDestination(int64_t index, bool internal, const CTxDestination& addr) override {}

    // ---- Ownership ----
    isminetype IsMine(const CScript& script) const override;
    bool HavePrivateKeys() const override;
    bool HaveCryptedKeys() const override;
    bool HaveP2MRKeys(const uint256& merkle_root) const EXCLUSIVE_LOCKS_REQUIRED(cs_p2mr);

    // ---- Metadata ----
    std::unique_ptr<CKeyMetadata> GetMetadata(const CTxDestination& dest) const override;
    const P2MRKeyMetadata* GetP2MRMetadata(const uint256& merkle_root) const;

    // ---- Signing ----
    bool SignTransaction(CMutableTransaction& tx,
                         const std::map<COutPoint, Coin>& coins,
                         int sighash,
                         std::map<int, bilingual_str>& input_errors) const override;

    bool SignP2MRInput(CMutableTransaction& tx, unsigned int input_idx,
                       const CScript& scriptPubKey, const CAmount& amount,
                       int sighash_type, SignatureData& sig_data,
                       P2MRSpendType spend_type) const EXCLUSIVE_LOCKS_REQUIRED(cs_p2mr);

    // ---- Encryption ----
    bool Encrypt(const CKeyingMaterial& master_key, WalletBatch* batch) override;
    bool CheckDecryptionKey(const CKeyingMaterial& master_key) override;

    // ---- Spend type preference ----
    void SetPreferredSpendType(P2MRSpendType type) { m_preferred_spend_type = type; }
    P2MRSpendType GetPreferredSpendType() const { return m_preferred_spend_type; }

    // ---- Utility ----
    size_t GetKeyCount() const;
    std::vector<uint256> GetAllMerkleRoots() const;
    std::unordered_set<CScript, SaltedSipHasher> GetScriptPubKeys() const override;

    // ---- Persistence ----
    bool LoadFromDB(WalletBatch& batch);
    bool WriteKey(WalletBatch& batch, const uint256& merkle_root,
                  const P2MRKeyMetadata& metadata) EXCLUSIVE_LOCKS_REQUIRED(cs_p2mr);

    // ---- String conversion ----
    static std::string SpendTypeToString(P2MRSpendType type);
    static std::optional<P2MRSpendType> StringToSpendType(const std::string& str);

    /** Estimate witness weight for a P2MR spend of a given type. */
    static int EstimateWitnessWeight(P2MRSpendType spend_type,
                                     const P2MRKeyMetadata& metadata);
};

} // namespace wallet

#endif // BITCOIN_WALLET_P2MR_SCRIPTPUBKEYMAN_H