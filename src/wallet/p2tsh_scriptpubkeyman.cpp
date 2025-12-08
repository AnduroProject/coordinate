#include <wallet/p2tsh_scriptpubkeyman.h>
#include <wallet/wallet.h>
#include <random.h>
#include <hash.h>
#include <script/sign.h>
#include <script/signingprovider.h>
#include <script/interpreter.h>  // For TAPROOT_LEAF_TAPSCRIPT
#include <key_io.h>
#include <logging.h>
#include <util/time.h>

// Use relative paths for secp256k1
#include "../secp256k1/include/secp256k1.h"
#include "../secp256k1/include/secp256k1_schnorrsig.h"
#include "../secp256k1/include/secp256k1_extrakeys.h"

extern "C" {
#include <libbitcoinpqc/slh_dsa.h>
}

namespace wallet {

P2TSHScriptPubKeyMan::P2TSHScriptPubKeyMan(WalletStorage& storage)
    : ScriptPubKeyMan(storage)
{
    secp_ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if (!secp_ctx) {
        throw std::runtime_error("Failed to create secp256k1 context");
    }
}

P2TSHScriptPubKeyMan::~P2TSHScriptPubKeyMan()
{
    if (secp_ctx) {
        secp256k1_context_destroy(secp_ctx);
        secp_ctx = nullptr;
    }
}

bool P2TSHScriptPubKeyMan::GenerateSchnorrKey(
    std::vector<unsigned char>& privkey,
    std::vector<unsigned char>& pubkey)
{
    privkey.resize(32);
    GetStrongRandBytes(privkey);
    
    secp256k1_keypair keypair;
    if (!secp256k1_keypair_create(secp_ctx, &keypair, privkey.data())) {
        WalletLogPrintf("P2TSH: Failed to create Schnorr keypair\n");
        return false;
    }
    
    secp256k1_xonly_pubkey xonly_pubkey;
    secp256k1_keypair_xonly_pub(secp_ctx, &xonly_pubkey, nullptr, &keypair);
    
    pubkey.resize(32);
    secp256k1_xonly_pubkey_serialize(secp_ctx, pubkey.data(), &xonly_pubkey);
    
    return true;
}

bool P2TSHScriptPubKeyMan::GenerateSLHDSAKey(
    std::vector<unsigned char>& privkey,
    std::vector<unsigned char>& pubkey)
{
    privkey.resize(SLH_DSA_SHAKE_128S_SECRET_KEY_SIZE);
    pubkey.resize(SLH_DSA_SHAKE_128S_PUBLIC_KEY_SIZE);
    
    std::vector<unsigned char> entropy(128);
    GetStrongRandBytes(entropy);
    
    int result = slh_dsa_shake_128s_keygen(
        pubkey.data(),
        privkey.data(),
        entropy.data(),
        entropy.size()
    );
    
    if (result != 0) {
        WalletLogPrintf("P2TSH: SLH-DSA key generation failed: %d\n", result);
        return false;
    }
    
    return true;
}

CScript P2TSHScriptPubKeyMan::BuildLeafScript(
    const std::vector<unsigned char>& schnorr_pk,
    const std::vector<unsigned char>& slh_dsa_pk,
    P2TSHSignatureType type) const
{
    CScript script;
    
    switch (type) {
        case P2TSHSignatureType::SCHNORR_ONLY:
            script << schnorr_pk << OP_CHECKSIG;
            break;
        case P2TSHSignatureType::SLH_DSA_ONLY:
            script << slh_dsa_pk << OP_SUBSTR;
            break;
        case P2TSHSignatureType::HYBRID:
            script << schnorr_pk << OP_CHECKSIG
                   << slh_dsa_pk << OP_SUBSTR
                   << OP_BOOLAND << OP_VERIFY;
            break;
    }
    
    return script;
}

util::Result<CTxDestination> P2TSHScriptPubKeyMan::GetNewP2TSHDestination(
    P2TSHSignatureType sig_type)
{
    LOCK(cs_p2tsh);
    
    std::vector<unsigned char> schnorr_priv, schnorr_pub;
    std::vector<unsigned char> slh_dsa_priv, slh_dsa_pub;
    
    // Generate keys based on signature type
    if (sig_type == P2TSHSignatureType::SCHNORR_ONLY ||
        sig_type == P2TSHSignatureType::HYBRID) {
        if (!GenerateSchnorrKey(schnorr_priv, schnorr_pub)) {
            return util::Error{Untranslated("Failed to generate Schnorr key")};
        }
    }
    
    if (sig_type == P2TSHSignatureType::SLH_DSA_ONLY ||
        sig_type == P2TSHSignatureType::HYBRID) {
        if (!GenerateSLHDSAKey(slh_dsa_priv, slh_dsa_pub)) {
            return util::Error{Untranslated("Failed to generate SLH-DSA key")};
        }
    }
    
    // Build leaf script
    CScript leaf_script = BuildLeafScript(schnorr_pub, slh_dsa_pub, sig_type);
    
    // Create tap tree using TaprootBuilder
    TaprootBuilder builder;
    builder.Add(0, leaf_script, TAPROOT_LEAF_TAPSCRIPT);
    
    // Finalize with dummy internal key (P2TSH doesn't use keypath)
    XOnlyPubKey dummy_key = XOnlyPubKey::NUMS_H;
    builder.Finalize(dummy_key);
    
    if (!builder.IsComplete()) {
        return util::Error{Untranslated("Failed to build tap tree")};
    }
    
    // Get merkle root and control block
    TaprootSpendData spend_data = builder.GetSpendData();
    uint256 merkle_root = spend_data.merkle_root;
    
    // Convert CScript to vector<unsigned char> and create key pair
    std::vector<unsigned char> script_bytes(leaf_script.begin(), leaf_script.end());
    auto script_key = std::make_pair(script_bytes, (int)TAPROOT_LEAF_TAPSCRIPT);
    
    auto control_it = spend_data.scripts.find(script_key);
    if (control_it == spend_data.scripts.end()) {
        return util::Error{Untranslated("Failed to get control block")};
    }
    
    // Get the first (shortest) control block from the set
    if (control_it->second.empty()) {
        return util::Error{Untranslated("No control block found")};
    }
    std::vector<unsigned char> control_block = *control_it->second.begin();
    
    // Create metadata
    P2TSHKeyMetadata metadata;
    metadata.sig_type = sig_type;
    metadata.schnorr_pubkey = schnorr_pub;
    metadata.slh_dsa_pubkey = slh_dsa_pub;
    metadata.merkle_root = merkle_root;
    metadata.leaf_script = leaf_script;
    metadata.control_block = control_block;
    metadata.create_time = GetTime();
    
    // Store keys
    if (!schnorr_priv.empty()) {
        CKeyID keyid = CKeyID(Hash160(schnorr_pub));
        mapSchnorrKeys[keyid] = schnorr_priv;
    }
    if (!slh_dsa_priv.empty()) {
        CKeyID keyid = CKeyID(Hash160(slh_dsa_pub));
        mapSLHDSAKeys[keyid] = slh_dsa_priv;
    }
    
    // Store metadata indexed by merkle root
    mapP2TSHMetadata[merkle_root] = metadata;
    
    // Write to database
    WalletBatch batch(m_storage.GetDatabase());
    WriteKey(batch, merkle_root, metadata);
    
    WalletLogPrintf("P2TSH: Generated new %s address (merkle root: %s)\n",
                   SignatureTypeToString(sig_type),
                   merkle_root.GetHex());
    
    // FIXED: Wrap WitnessV2P2TSH in CTxDestination
    return CTxDestination(WitnessV2P2TSH(merkle_root));
}

util::Result<CTxDestination> P2TSHScriptPubKeyMan::GetNewDestination(const OutputType type)
{
    return GetNewP2TSHDestination(m_default_sig_type);
}

isminetype P2TSHScriptPubKeyMan::IsMine(const CScript& script) const
{
    LOCK(cs_p2tsh);
    
    if (script.size() != 34 || script[0] != OP_2 || script[1] != 0x20) {
        return ISMINE_NO;
    }
    
    uint256 merkle_root;
    std::copy(script.begin() + 2, script.begin() + 34, merkle_root.begin());
    
    if (mapP2TSHMetadata.find(merkle_root) != mapP2TSHMetadata.end()) {
        return ISMINE_SPENDABLE;
    }
    
    return ISMINE_NO;
}

bool P2TSHScriptPubKeyMan::HavePrivateKeys() const
{
    LOCK(cs_p2tsh);
    return !mapSchnorrKeys.empty() || !mapSLHDSAKeys.empty();
}

bool P2TSHScriptPubKeyMan::HaveCryptedKeys() const
{
    LOCK(cs_p2tsh);
    return !mapCryptedSchnorrKeys.empty() || !mapCryptedSLHDSAKeys.empty();
}

std::unique_ptr<CKeyMetadata> P2TSHScriptPubKeyMan::GetMetadata(const CTxDestination& dest) const
{
    if (!std::holds_alternative<WitnessV2P2TSH>(dest)) {
        return nullptr;
    }
    
    const WitnessV2P2TSH& p2tsh = std::get<WitnessV2P2TSH>(dest);
    uint256 merkle_root;
    std::copy(p2tsh.begin(), p2tsh.end(), merkle_root.begin());
    
    const P2TSHKeyMetadata* p2tsh_meta = GetP2TSHMetadata(merkle_root);
    if (!p2tsh_meta) return nullptr;
    
    auto meta = std::make_unique<CKeyMetadata>();
    meta->nCreateTime = p2tsh_meta->create_time;
    return meta;
}

const P2TSHKeyMetadata* P2TSHScriptPubKeyMan::GetP2TSHMetadata(const uint256& merkle_root) const
{
    LOCK(cs_p2tsh);
    auto it = mapP2TSHMetadata.find(merkle_root);
    return (it != mapP2TSHMetadata.end()) ? &it->second : nullptr;
}

size_t P2TSHScriptPubKeyMan::GetKeyCount() const
{
    LOCK(cs_p2tsh);
    return mapP2TSHMetadata.size();
}

std::vector<uint256> P2TSHScriptPubKeyMan::GetAllMerkleRoots() const
{
    LOCK(cs_p2tsh);
    std::vector<uint256> roots;
    roots.reserve(mapP2TSHMetadata.size());
    for (const auto& [root, metadata] : mapP2TSHMetadata) {
        roots.push_back(root);
    }
    return roots;
}

bool P2TSHScriptPubKeyMan::SignTransaction(
    CMutableTransaction& tx,
    const std::map<COutPoint, Coin>& coins,
    int sighash_type,
    std::map<int, bilingual_str>& input_errors) const
{
    WalletLogPrintf("P2TSH: SignTransaction called for %d inputs\n", tx.vin.size());
    return true;
}

bool P2TSHScriptPubKeyMan::SignP2TSHInput(
    CMutableTransaction& tx,
    unsigned int input_idx,
    const CScript& scriptPubKey,
    const CAmount& amount,
    int sighash_type,
    SignatureData& sig_data) const
{
    return false;
}

bool P2TSHScriptPubKeyMan::Encrypt(const CKeyingMaterial& master_key, WalletBatch* batch)
{
    return false;
}

bool P2TSHScriptPubKeyMan::CheckDecryptionKey(const CKeyingMaterial& master_key)
{
    return false;
}

bool P2TSHScriptPubKeyMan::WriteKey(
    WalletBatch& batch,
    const uint256& merkle_root,
    const P2TSHKeyMetadata& metadata)
{
    WalletLogPrintf("P2TSH: WriteKey called for merkle root %s\n", merkle_root.GetHex());
    return true;
}

bool P2TSHScriptPubKeyMan::LoadFromDB(WalletBatch& batch)
{
    return true;
}

std::string P2TSHScriptPubKeyMan::SignatureTypeToString(P2TSHSignatureType type) const
{
    switch (type) {
        case P2TSHSignatureType::SCHNORR_ONLY: return "schnorr";
        case P2TSHSignatureType::SLH_DSA_ONLY: return "slhdsa";
        case P2TSHSignatureType::HYBRID: return "hybrid";
        default: return "unknown";
    }
}

std::optional<P2TSHSignatureType> P2TSHScriptPubKeyMan::StringToSignatureType(const std::string& str)
{
    if (str == "schnorr") return P2TSHSignatureType::SCHNORR_ONLY;
    if (str == "slhdsa") return P2TSHSignatureType::SLH_DSA_ONLY;
    if (str == "hybrid") return P2TSHSignatureType::HYBRID;
    return std::nullopt;
}

} // namespace wallet