// Copyright (c) 2024 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_P2TSH_SCRIPTPUBKEYMAN_H
#define BITCOIN_WALLET_P2TSH_SCRIPTPUBKEYMAN_H

#include <wallet/scriptpubkeyman.h>
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

enum class P2TSHSignatureType : uint8_t {
    SCHNORR_ONLY = 0,
    SLH_DSA_ONLY = 1,
    HYBRID = 2
};

struct P2TSHKeyMetadata {
    P2TSHSignatureType sig_type{P2TSHSignatureType::HYBRID};
    std::vector<unsigned char> schnorr_pubkey;
    std::vector<unsigned char> slh_dsa_pubkey;
    uint256 merkle_root;
    CScript leaf_script;
    std::vector<unsigned char> control_block;
    int64_t create_time{0};
    
    SERIALIZE_METHODS(P2TSHKeyMetadata, obj) {
        uint8_t sig_type_val = static_cast<uint8_t>(obj.sig_type);
        READWRITE(sig_type_val);
        if constexpr (!std::is_const_v<std::remove_reference_t<decltype(obj)>>) {
            obj.sig_type = static_cast<P2TSHSignatureType>(sig_type_val);
        }
        READWRITE(obj.schnorr_pubkey, obj.slh_dsa_pubkey);
        READWRITE(obj.merkle_root, obj.leaf_script);
        READWRITE(obj.control_block, obj.create_time);
    }
};

class P2TSHScriptPubKeyMan : public ScriptPubKeyMan
{
private:
    mutable RecursiveMutex cs_p2tsh;
    std::map<CKeyID, std::vector<unsigned char>> mapSchnorrKeys GUARDED_BY(cs_p2tsh);
    std::map<CKeyID, std::vector<unsigned char>> mapSLHDSAKeys GUARDED_BY(cs_p2tsh);
    std::map<CKeyID, std::vector<unsigned char>> mapCryptedSchnorrKeys GUARDED_BY(cs_p2tsh);
    std::map<CKeyID, std::vector<unsigned char>> mapCryptedSLHDSAKeys GUARDED_BY(cs_p2tsh);
    std::map<uint256, P2TSHKeyMetadata> mapP2TSHMetadata GUARDED_BY(cs_p2tsh);
    P2TSHSignatureType m_default_sig_type{P2TSHSignatureType::HYBRID};
    secp256k1_context* secp_ctx{nullptr};
    
    bool GenerateSchnorrKey(std::vector<unsigned char>& priv, std::vector<unsigned char>& pub);
    bool GenerateSLHDSAKey(std::vector<unsigned char>& priv, std::vector<unsigned char>& pub);
    CScript BuildLeafScript(const std::vector<unsigned char>& schnorr_pk,
                           const std::vector<unsigned char>& slh_dsa_pk,
                           P2TSHSignatureType type) const;

public:
    explicit P2TSHScriptPubKeyMan(WalletStorage& storage);
    virtual ~P2TSHScriptPubKeyMan();
    
    util::Result<CTxDestination> GetNewDestination(const OutputType type) override;
    util::Result<CTxDestination> GetNewP2TSHDestination(P2TSHSignatureType sig_type);
    
    isminetype IsMine(const CScript& script) const override;
    bool HavePrivateKeys() const override;
    bool HaveCryptedKeys() const override;
    
    // FIXED: Override base class method properly
    std::unique_ptr<CKeyMetadata> GetMetadata(const CTxDestination& dest) const override;
    
    // P2TSH-specific metadata getter
    const P2TSHKeyMetadata* GetP2TSHMetadata(const uint256& merkle_root) const;
    
    bool SignTransaction(CMutableTransaction& tx, 
                        const std::map<COutPoint, Coin>& coins,
                        int sighash,
                        std::map<int, bilingual_str>& input_errors) const override;
    
    bool SignP2TSHInput(CMutableTransaction& tx, unsigned int input_idx,
                       const CScript& scriptPubKey, const CAmount& amount,
                       int sighash_type, SignatureData& sig_data) const;
    
    bool Encrypt(const CKeyingMaterial& master_key, WalletBatch* batch) override;
    bool CheckDecryptionKey(const CKeyingMaterial& master_key) override;
    
    void SetDefaultSignatureType(P2TSHSignatureType type) { m_default_sig_type = type; }
    P2TSHSignatureType GetDefaultSignatureType() const { return m_default_sig_type; }
    
    size_t GetKeyCount() const;
    std::vector<uint256> GetAllMerkleRoots() const;
    
    bool LoadFromDB(WalletBatch& batch);
    bool WriteKey(WalletBatch& batch, const uint256& merkle_root, const P2TSHKeyMetadata& metadata);
    
    std::string SignatureTypeToString(P2TSHSignatureType type) const;
    static std::optional<P2TSHSignatureType> StringToSignatureType(const std::string& str);
};

} // namespace wallet

#endif // BITCOIN_WALLET_P2TSH_SCRIPTPUBKEYMAN_H