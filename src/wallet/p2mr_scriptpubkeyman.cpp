// Copyright (c) 2024-2026 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/p2mr_scriptpubkeyman.h>
#include <wallet/wallet.h>
#include <random.h>
#include <hash.h>
#include <script/sign.h>
#include <script/signingprovider.h>
#include <script/interpreter.h>
#include <key_io.h>
#include <logging.h>
#include <util/strencodings.h>
#include <util/time.h>

#include "../secp256k1/include/secp256k1.h"
#include "../secp256k1/include/secp256k1_schnorrsig.h"
#include "../secp256k1/include/secp256k1_extrakeys.h"

extern "C" {
#include <libbitcoinpqc/slh_dsa.h>
}

namespace wallet {

static constexpr uint8_t P2MR_CONTROL_BYTE = 0xc1;

// ============================================================================
// Construction / Destruction
// ============================================================================

P2MRScriptPubKeyMan::P2MRScriptPubKeyMan(WalletStorage& storage)
    : ScriptPubKeyMan(storage)
{
    secp_ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if (!secp_ctx) {
        throw std::runtime_error("Failed to create secp256k1 context");
    }
}

P2MRScriptPubKeyMan::~P2MRScriptPubKeyMan()
{
    if (secp_ctx) {
        secp256k1_context_destroy(secp_ctx);
        secp_ctx = nullptr;
    }
}

// ============================================================================
// Key Generation
// ============================================================================

bool P2MRScriptPubKeyMan::GenerateSchnorrKey(
    std::vector<unsigned char>& privkey,
    std::vector<unsigned char>& pubkey)
{
    privkey.resize(32);
    GetStrongRandBytes(privkey);

    secp256k1_keypair keypair;
    if (!secp256k1_keypair_create(secp_ctx, &keypair, privkey.data())) {
        return false;
    }

    secp256k1_xonly_pubkey xonly_pubkey;
    secp256k1_keypair_xonly_pub(secp_ctx, &xonly_pubkey, nullptr, &keypair);

    pubkey.resize(32);
    secp256k1_xonly_pubkey_serialize(secp_ctx, pubkey.data(), &xonly_pubkey);
    return true;
}

bool P2MRScriptPubKeyMan::GenerateSLHDSAKey(
    std::vector<unsigned char>& privkey,
    std::vector<unsigned char>& pubkey)
{
    privkey.resize(SLH_DSA_SHAKE_128S_SECRET_KEY_SIZE);
    pubkey.resize(SLH_DSA_SHAKE_128S_PUBLIC_KEY_SIZE);

    std::vector<unsigned char> entropy(128);
    for (size_t i = 0; i < 128; i += 32) {
        size_t chunk = std::min<size_t>(32, 128 - i);
        std::vector<unsigned char> buf(chunk);
        GetStrongRandBytes(buf);
        std::copy(buf.begin(), buf.end(), entropy.begin() + i);
    }

    int result = slh_dsa_shake_128s_keygen(
        pubkey.data(), privkey.data(), entropy.data(), entropy.size());

    if (result != 0) {
        WalletLogPrintf("P2MR: SLH-DSA key generation failed: %d\n", result);
        return false;
    }
    return true;
}

// ============================================================================
// Leaf Script Construction
// ============================================================================

CScript P2MRScriptPubKeyMan::BuildSchnorrLeafScript(
    const std::vector<unsigned char>& schnorr_pk)
{
    CScript script;
    script << schnorr_pk << OP_CHECKSIG;
    return script;
}

CScript P2MRScriptPubKeyMan::BuildSLHDSALeafScript(
    const std::vector<unsigned char>& slh_dsa_pk)
{
    CScript script;
    script << slh_dsa_pk << OP_SUBSTR;
    return script;
}

CScript P2MRScriptPubKeyMan::BuildHybridLeafScript(
    const std::vector<unsigned char>& schnorr_pk,
    const std::vector<unsigned char>& slh_dsa_pk)
{
    CScript script;
    script << schnorr_pk << OP_CHECKSIG
           << slh_dsa_pk << OP_SUBSTR
           << OP_BOOLAND << OP_VERIFY;
    return script;
}

// ============================================================================
// 3-Leaf Tree Construction
//
// Tree layout:
//            merkle_root
//           /            \
//        branch          leaf3 (hybrid)
//       /      \
//   leaf1       leaf2
// (schnorr)   (slh-dsa)
//
// Control blocks (P2MR: no internal pubkey):
//   leaf1: 0xc1 | leaf2_hash | leaf3_hash   (65 bytes)
//   leaf2: 0xc1 | leaf1_hash | leaf3_hash   (65 bytes)
//   leaf3: 0xc1 | branch_hash              (33 bytes)
// ============================================================================

bool P2MRScriptPubKeyMan::BuildP2MRTree(
    const std::vector<unsigned char>& schnorr_pk,
    const std::vector<unsigned char>& slh_dsa_pk,
    P2MRKeyMetadata& metadata) const
{
    CScript schnorr_script = BuildSchnorrLeafScript(schnorr_pk);
    CScript slhdsa_script  = BuildSLHDSALeafScript(slh_dsa_pk);
    CScript hybrid_script  = BuildHybridLeafScript(schnorr_pk, slh_dsa_pk);

    // TapLeaf hashes
    uint256 leaf1_hash = (HashWriter{HASHER_TAPLEAF}
        << uint8_t(TAPROOT_LEAF_TAPSCRIPT) << schnorr_script).GetSHA256();

    uint256 leaf2_hash = (HashWriter{HASHER_TAPLEAF}
        << uint8_t(TAPROOT_LEAF_TAPSCRIPT) << slhdsa_script).GetSHA256();

    uint256 leaf3_hash = (HashWriter{HASHER_TAPLEAF}
        << uint8_t(TAPROOT_LEAF_TAPSCRIPT) << hybrid_script).GetSHA256();

    // Branch = TapBranch(sorted(leaf1, leaf2))
    uint256 branch_hash;
    if (leaf1_hash < leaf2_hash) {
        branch_hash = (HashWriter{HASHER_TAPBRANCH} << leaf1_hash << leaf2_hash).GetSHA256();
    } else {
        branch_hash = (HashWriter{HASHER_TAPBRANCH} << leaf2_hash << leaf1_hash).GetSHA256();
    }

    // Root = TapBranch(sorted(branch, leaf3))
    uint256 root_hash;
    if (branch_hash < leaf3_hash) {
        root_hash = (HashWriter{HASHER_TAPBRANCH} << branch_hash << leaf3_hash).GetSHA256();
    } else {
        root_hash = (HashWriter{HASHER_TAPBRANCH} << leaf3_hash << branch_hash).GetSHA256();
    }

    metadata.merkle_root = root_hash;

    // Leaf 1 (Schnorr): path = [leaf2_hash, leaf3_hash]
    {
        P2MRLeafInfo& info = metadata.leaves[static_cast<uint8_t>(P2MRSpendType::SCHNORR)];
        info.script = schnorr_script;
        info.leaf_hash = leaf1_hash;
        info.control_block.clear();
        info.control_block.push_back(P2MR_CONTROL_BYTE);
        info.control_block.insert(info.control_block.end(), leaf2_hash.begin(), leaf2_hash.end());
        info.control_block.insert(info.control_block.end(), leaf3_hash.begin(), leaf3_hash.end());
    }

    // Leaf 2 (SLH-DSA): path = [leaf1_hash, leaf3_hash]
    {
        P2MRLeafInfo& info = metadata.leaves[static_cast<uint8_t>(P2MRSpendType::SLH_DSA)];
        info.script = slhdsa_script;
        info.leaf_hash = leaf2_hash;
        info.control_block.clear();
        info.control_block.push_back(P2MR_CONTROL_BYTE);
        info.control_block.insert(info.control_block.end(), leaf1_hash.begin(), leaf1_hash.end());
        info.control_block.insert(info.control_block.end(), leaf3_hash.begin(), leaf3_hash.end());
    }

    // Leaf 3 (Hybrid): path = [branch_hash]
    {
        P2MRLeafInfo& info = metadata.leaves[static_cast<uint8_t>(P2MRSpendType::HYBRID)];
        info.script = hybrid_script;
        info.leaf_hash = leaf3_hash;
        info.control_block.clear();
        info.control_block.push_back(P2MR_CONTROL_BYTE);
        info.control_block.insert(info.control_block.end(), branch_hash.begin(), branch_hash.end());
    }

    WalletLogPrintf("P2MR: Built 3-leaf tree (root: %s)\n", root_hash.GetHex());
    return true;
}

// ============================================================================
// Address Generation
// ============================================================================

util::Result<CTxDestination> P2MRScriptPubKeyMan::GetNewP2MRDestination()
{
    LOCK(cs_p2mr);

    std::vector<unsigned char> schnorr_priv, schnorr_pub;
    std::vector<unsigned char> slh_dsa_priv, slh_dsa_pub;

    if (m_pqhd && m_pqhd->IsSeeded()) {
        // ---- PQHD deterministic derivation ----
        PQHDKeyPair schnorr_pair, slhdsa_pair;
        if (!m_pqhd->DeriveP2MRKeyPairs(m_coin_type, m_account,
                                          PQHD_CHANGE_EXTERNAL,
                                          m_next_external_index,
                                          schnorr_pair, slhdsa_pair)) {
            return util::Error{Untranslated(strprintf(
                "PQHD derivation failed at index %u", m_next_external_index))};
        }

        schnorr_priv = schnorr_pair.secret_key;
        schnorr_pub = schnorr_pair.public_key;
        slh_dsa_priv = slhdsa_pair.secret_key;
        slh_dsa_pub = slhdsa_pair.public_key;

        WalletLogPrintf("P2MR: PQHD derived keys at m/360'/%u'/%u'/[340,205]'/0'/%u\n",
                        m_coin_type, m_account, m_next_external_index);
        m_next_external_index++;
    } else {
        // ---- Random key generation (non-HD fallback) ----
        if (!GenerateSchnorrKey(schnorr_priv, schnorr_pub)) {
            return util::Error{Untranslated("Failed to generate Schnorr key")};
        }
        if (!GenerateSLHDSAKey(slh_dsa_priv, slh_dsa_pub)) {
            return util::Error{Untranslated("Failed to generate SLH-DSA key")};
        }
        WalletLogPrintf("P2MR: Generated random keys (non-HD)\n");
    }

    P2MRKeyMetadata metadata;
    metadata.schnorr_pubkey = schnorr_pub;
    metadata.slh_dsa_pubkey = slh_dsa_pub;
    metadata.create_time = GetTime();

    if (!BuildP2MRTree(schnorr_pub, slh_dsa_pub, metadata)) {
        return util::Error{Untranslated("Failed to build P2MR tree")};
    }

    // Store keys
    mapSchnorrKeys[CKeyID(Hash160(schnorr_pub))] = schnorr_priv;
    mapSLHDSAKeys[CKeyID(Hash160(slh_dsa_pub))] = slh_dsa_priv;
    mapP2MRMetadata[metadata.merkle_root] = metadata;

    // Persist
    WalletBatch batch(m_storage.GetDatabase());
    WriteKey(batch, metadata.merkle_root, metadata);

    // Register destination
    WitnessV2P2MR p2mr_dest(metadata.merkle_root);
    CTxDestination dest(p2mr_dest);
    CScript spk = GetScriptForDestination(dest);
    {
        CWallet& wallet = static_cast<CWallet&>(m_storage);
        LOCK(wallet.cs_wallet);
        // Register in the wallet's script cache so IsMine/coin selection work
        wallet.TopUpCallback({spk}, this);
    }

    WalletLogPrintf("P2MR: New address (root: %s) with 3 spend paths\n",
                    metadata.merkle_root.GetHex());
    return dest;
}

util::Result<CTxDestination> P2MRScriptPubKeyMan::GetNewDestination(const OutputType type)
{
    return GetNewP2MRDestination();
}

// ============================================================================
// PQHD Seed-based Derivation
// ============================================================================

bool P2MRScriptPubKeyMan::SetSeed(
    const std::vector<unsigned char>& bip39_seed,
    uint32_t coin_type, uint32_t account)
{
    LOCK(cs_p2mr);

    m_pqhd = std::make_unique<PQHDKeyDerivation>();
    if (!m_pqhd->SetSeed(bip39_seed)) {
        m_pqhd.reset();
        return false;
    }

    m_coin_type = coin_type;
    m_account = account;
    m_next_external_index = 0;
    m_next_internal_index = 0;

    WalletLogPrintf("P2MR: PQHD initialized (coin_type=%u, account=%u)\n",
                    coin_type, account);
    return true;
}

bool P2MRScriptPubKeyMan::IsHDEnabled() const
{
    LOCK(cs_p2mr);
    return m_pqhd && m_pqhd->IsSeeded();
}

std::string P2MRScriptPubKeyMan::GetDescriptorString(const uint256& merkle_root) const
{
    LOCK(cs_p2mr);

    auto it = mapP2MRMetadata.find(merkle_root);
    if (it == mapP2MRMetadata.end()) return "";

    const P2MRKeyMetadata& meta = it->second;

    // For now, produce a concrete descriptor with literal pubkeys
    // e.g.: p2mr(ac65830ba524dd01..., f38969a30c004599...)
    return "p2mr(" + HexStr(meta.schnorr_pubkey) + "," +
                     HexStr(meta.slh_dsa_pubkey) + ")";
}

// ============================================================================
// Standard Wallet Interface
// ============================================================================

bool P2MRScriptPubKeyMan::CanProvide(const CScript& script, SignatureData& sigdata)
{
    LOCK(cs_p2mr);
    if (script.size() != 34 || script[0] != OP_2 || script[1] != 0x20) return false;
    uint256 root;
    std::copy(script.begin() + 2, script.begin() + 34, root.begin());
    return mapP2MRMetadata.count(root) > 0;
}

util::Result<CTxDestination> P2MRScriptPubKeyMan::GetReservedDestination(
    const OutputType type, bool internal, int64_t& index)
{
    auto dest = GetNewP2MRDestination();
    if (!dest) return dest;
    index = 0;  // P2MR doesn't use keypool indices
    return dest;
}

// ============================================================================
// Ownership
// ============================================================================

isminetype P2MRScriptPubKeyMan::IsMine(const CScript& script) const
{
    LOCK(cs_p2mr);
    if (script.size() != 34 || script[0] != OP_2 || script[1] != 0x20) return ISMINE_NO;

    uint256 merkle_root;
    std::copy(script.begin() + 2, script.begin() + 34, merkle_root.begin());
    return mapP2MRMetadata.count(merkle_root) ? ISMINE_SPENDABLE : ISMINE_NO;
}

bool P2MRScriptPubKeyMan::HavePrivateKeys() const
{
    LOCK(cs_p2mr);
    return !mapSchnorrKeys.empty() || !mapSLHDSAKeys.empty();
}

bool P2MRScriptPubKeyMan::HaveCryptedKeys() const
{
    LOCK(cs_p2mr);
    return !mapCryptedSchnorrKeys.empty() || !mapCryptedSLHDSAKeys.empty();
}

bool P2MRScriptPubKeyMan::HaveP2MRKeys(const uint256& merkle_root) const
{
    AssertLockHeld(cs_p2mr);
    return mapP2MRMetadata.count(merkle_root) > 0;
}

// ============================================================================
// Metadata
// ============================================================================

std::unique_ptr<CKeyMetadata> P2MRScriptPubKeyMan::GetMetadata(const CTxDestination& dest) const
{
    if (!std::holds_alternative<WitnessV2P2MR>(dest)) return nullptr;

    const auto& p2mr = std::get<WitnessV2P2MR>(dest);
    uint256 merkle_root;
    std::copy(p2mr.begin(), p2mr.end(), merkle_root.begin());

    const P2MRKeyMetadata* meta = GetP2MRMetadata(merkle_root);
    if (!meta) return nullptr;

    auto result = std::make_unique<CKeyMetadata>();
    result->nCreateTime = meta->create_time;
    return result;
}

const P2MRKeyMetadata* P2MRScriptPubKeyMan::GetP2MRMetadata(const uint256& merkle_root) const
{
    LOCK(cs_p2mr);
    auto it = mapP2MRMetadata.find(merkle_root);
    return (it != mapP2MRMetadata.end()) ? &it->second : nullptr;
}

size_t P2MRScriptPubKeyMan::GetKeyCount() const
{
    LOCK(cs_p2mr);
    return mapP2MRMetadata.size();
}

std::vector<uint256> P2MRScriptPubKeyMan::GetAllMerkleRoots() const
{
    LOCK(cs_p2mr);
    std::vector<uint256> roots;
    roots.reserve(mapP2MRMetadata.size());
    for (const auto& [root, _] : mapP2MRMetadata) roots.push_back(root);
    return roots;
}

// ============================================================================
// Signing
// ============================================================================

bool P2MRScriptPubKeyMan::SignSchnorr(
    const std::vector<unsigned char>& privkey,
    const uint256& sighash,
    std::vector<unsigned char>& signature) const
{
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    secp256k1_keypair keypair;
    if (!secp256k1_keypair_create(ctx, &keypair, privkey.data())) {
        secp256k1_context_destroy(ctx);
        return false;
    }
    signature.resize(64);
    if (!secp256k1_schnorrsig_sign32(ctx, signature.data(), sighash.begin(), &keypair, nullptr)) {
        secp256k1_context_destroy(ctx);
        return false;
    }
    secp256k1_context_destroy(ctx);
    return true;
}

bool P2MRScriptPubKeyMan::SignSLHDSA(
    const std::vector<unsigned char>& privkey,
    const uint256& sighash,
    std::vector<unsigned char>& signature) const
{
    if (privkey.size() != SLH_DSA_SHAKE_128S_SECRET_KEY_SIZE) return false;

    signature.resize(SLH_DSA_SHAKE_128S_SIGNATURE_SIZE);
    size_t siglen = SLH_DSA_SHAKE_128S_SIGNATURE_SIZE;

    int result = slh_dsa_shake_128s_sign(
        signature.data(), &siglen, sighash.begin(), 32, privkey.data());
    if (result != 0) return false;

    signature.resize(siglen);
    signature.push_back(SIGHASH_DEFAULT);
    return true;
}

bool P2MRScriptPubKeyMan::SignTransaction(
    CMutableTransaction& tx,
    const std::map<COutPoint, Coin>& coins,
    int sighash_type,
    std::map<int, bilingual_str>& input_errors) const
{
    LOCK(cs_p2mr);
    bool all_signed = true;

    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        auto coin_it = coins.find(tx.vin[i].prevout);
        if (coin_it == coins.end()) continue;

        const CScript& spk = coin_it->second.out.scriptPubKey;
        if (spk.size() != 34 || spk[0] != OP_2 || spk[1] != 0x20) continue;

        uint256 merkle_root;
        std::copy(spk.begin() + 2, spk.begin() + 34, merkle_root.begin());
        if (!mapP2MRMetadata.count(merkle_root)) continue;

        SignatureData sig_data;
        if (!SignP2MRInput(tx, i, spk, coin_it->second.out.nValue,
                           sighash_type, sig_data, m_preferred_spend_type)) {
            input_errors[i] = Untranslated(strprintf("P2MR signing failed for input %d", i));
            all_signed = false;
        }
    }
    return all_signed;
}

bool P2MRScriptPubKeyMan::SignP2MRInput(
    CMutableTransaction& tx, unsigned int input_idx,
    const CScript& scriptPubKey, const CAmount& amount,
    int sighash_type, SignatureData& sig_data,
    P2MRSpendType spend_type) const
{
    uint256 merkle_root;
    std::copy(scriptPubKey.begin() + 2, scriptPubKey.begin() + 34, merkle_root.begin());

    auto meta_it = mapP2MRMetadata.find(merkle_root);
    if (meta_it == mapP2MRMetadata.end()) return false;
    const P2MRKeyMetadata& metadata = meta_it->second;
    const P2MRLeafInfo& leaf = metadata.GetLeaf(spend_type);

    CWallet& wallet = static_cast<CWallet&>(m_storage);
    LOCK(wallet.cs_wallet);

    // Build spent outputs for sighash
    std::vector<CTxOut> spent_outputs;
    spent_outputs.reserve(tx.vin.size());
    for (size_t i = 0; i < tx.vin.size(); ++i) {
        if (i == input_idx) {
            spent_outputs.push_back(CTxOut(amount, scriptPubKey));
        } else {
            const CWalletTx* prev = wallet.GetWalletTx(tx.vin[i].prevout.hash);
            if (prev && tx.vin[i].prevout.n < prev->tx->vout.size()) {
                spent_outputs.push_back(prev->tx->vout[tx.vin[i].prevout.n]);
            } else {
                return false;
            }
        }
    }

    PrecomputedTransactionData txdata;
    txdata.Init(tx, std::move(spent_outputs), true);

    ScriptExecutionData execdata;
    execdata.m_annex_init = true;
    execdata.m_annex_present = false;
    execdata.m_codeseparator_pos_init = true;
    execdata.m_codeseparator_pos = 0xFFFFFFFF;
    execdata.m_tapleaf_hash_init = true;
    execdata.m_tapleaf_hash = leaf.leaf_hash;

    int actual_sighash = (spend_type == P2MRSpendType::HYBRID) ? SIGHASH_ALL : sighash_type;

    uint256 sighash;
    if (!SignatureHashSchnorr(sighash, execdata, tx, input_idx,
                              actual_sighash, SigVersion::TAPSCRIPT,
                              txdata, MissingDataBehavior::FAIL)) {
        return false;
    }

    CScriptWitness witness;

    switch (spend_type) {
    case P2MRSpendType::SCHNORR: {
        auto it = mapSchnorrKeys.find(CKeyID(Hash160(metadata.schnorr_pubkey)));
        if (it == mapSchnorrKeys.end()) return false;
        std::vector<unsigned char> sig;
        if (!SignSchnorr(it->second, sighash, sig)) return false;
        witness.stack.push_back(sig);
        break;
    }
    case P2MRSpendType::SLH_DSA: {
        auto it = mapSLHDSAKeys.find(CKeyID(Hash160(metadata.slh_dsa_pubkey)));
        if (it == mapSLHDSAKeys.end()) return false;
        std::vector<unsigned char> sig;
        if (!SignSLHDSA(it->second, sighash, sig)) return false;
        witness.stack.push_back(sig);
        break;
    }
    case P2MRSpendType::HYBRID: {
        auto schnorr_it = mapSchnorrKeys.find(CKeyID(Hash160(metadata.schnorr_pubkey)));
        auto slhdsa_it = mapSLHDSAKeys.find(CKeyID(Hash160(metadata.slh_dsa_pubkey)));
        if (schnorr_it == mapSchnorrKeys.end() || slhdsa_it == mapSLHDSAKeys.end()) return false;

        std::vector<unsigned char> schnorr_sig, slhdsa_sig;
        if (!SignSchnorr(schnorr_it->second, sighash, schnorr_sig)) return false;
        if (!SignSLHDSA(slhdsa_it->second, sighash, slhdsa_sig)) return false;

        if (!slhdsa_sig.empty() && slhdsa_sig.back() == SIGHASH_DEFAULT) {
            slhdsa_sig.pop_back();
        }

        // Witness stack: [schnorr_sig, slhdsa_sig, leaf_script, control_block]
        // After VerifyWitnessProgram pops leaf_script + control_block:
        //   stack[0] = schnorr_sig (64 bytes), stack[1] = slhdsa_sig (7856 bytes)
        // This matches the interpreter's HandleSLHDSASignature expectation.
        witness.stack.push_back(schnorr_sig);
        witness.stack.push_back(slhdsa_sig);
        break;
    }
    }

    // Append leaf script + control block
    witness.stack.push_back(std::vector<unsigned char>(leaf.script.begin(), leaf.script.end()));
    witness.stack.push_back(leaf.control_block);

    tx.vin[input_idx].scriptWitness = witness;
    sig_data.complete = true;

    WalletLogPrintf("P2MR: Signed input %d via %s path\n",
                    input_idx, SpendTypeToString(spend_type));
    return true;
}

// ============================================================================
// Encryption (stub)
// ============================================================================

bool P2MRScriptPubKeyMan::Encrypt(const CKeyingMaterial& master_key, WalletBatch* batch) { return false; }
bool P2MRScriptPubKeyMan::CheckDecryptionKey(const CKeyingMaterial& master_key) { return false; }

// ============================================================================
// Persistence
// ============================================================================

bool P2MRScriptPubKeyMan::WriteKey(
    WalletBatch& batch, const uint256& merkle_root, const P2MRKeyMetadata& metadata)
{
    AssertLockHeld(cs_p2mr);
    if (!batch.WriteP2MRMetadata(merkle_root, metadata)) return false;

    if (!metadata.schnorr_pubkey.empty()) {
        CKeyID kid = CKeyID(Hash160(metadata.schnorr_pubkey));
        auto it = mapSchnorrKeys.find(kid);
        if (it != mapSchnorrKeys.end() && !batch.WriteP2MRSchnorrKey(kid, it->second)) return false;
    }
    if (!metadata.slh_dsa_pubkey.empty()) {
        CKeyID kid = CKeyID(Hash160(metadata.slh_dsa_pubkey));
        auto it = mapSLHDSAKeys.find(kid);
        if (it != mapSLHDSAKeys.end() && !batch.WriteP2MRSLHDSAKey(kid, it->second)) return false;
    }
    return true;
}

bool P2MRScriptPubKeyMan::LoadFromDB(WalletBatch& batch)
{
    LOCK(cs_p2mr);
    size_t meta_count = 0, schnorr_count = 0, slhdsa_count = 0;

    std::unique_ptr<DatabaseCursor> cursor = batch.GetNewCursor();
    if (!cursor) return false;

    DatabaseCursor::Status status = DatabaseCursor::Status::MORE;
    while (status == DatabaseCursor::Status::MORE) {
        DataStream key{}, value{};
        status = cursor->Next(key, value);
        if (status != DatabaseCursor::Status::MORE) break;

        std::string type;
        key >> type;
        try {
            if (type == DBKeys::P2MR_METADATA) {
                uint256 root; key >> root;
                P2MRKeyMetadata meta; value >> meta;
                mapP2MRMetadata[root] = meta;
                meta_count++;
            } else if (type == DBKeys::P2MR_SCHNORR_KEY) {
                CKeyID kid; key >> kid;
                std::vector<unsigned char> pk; value >> pk;
                mapSchnorrKeys[kid] = pk;
                schnorr_count++;
            } else if (type == DBKeys::P2MR_SLHDSA_KEY) {
                CKeyID kid; key >> kid;
                std::vector<unsigned char> pk; value >> pk;
                mapSLHDSAKeys[kid] = pk;
                slhdsa_count++;
            }
        } catch (const std::exception& e) {
            WalletLogPrintf("P2MR: Error loading: %s\n", e.what());
        }
    }

    WalletLogPrintf("P2MR: Loaded %zu addresses, %zu schnorr, %zu slh-dsa keys\n",
                    meta_count, schnorr_count, slhdsa_count);
    return true;
}

// ============================================================================
// ScriptPubKeys
// ============================================================================

std::unordered_set<CScript, SaltedSipHasher> P2MRScriptPubKeyMan::GetScriptPubKeys() const
{
    LOCK(cs_p2mr);
    std::unordered_set<CScript, SaltedSipHasher> scripts;
    for (const auto& [root, _] : mapP2MRMetadata) {
        scripts.insert(GetScriptForDestination(CTxDestination(WitnessV2P2MR(root))));
    }
    return scripts;
}

// ============================================================================
// Witness Weight Estimation
// ============================================================================

int P2MRScriptPubKeyMan::EstimateWitnessWeight(
    P2MRSpendType spend_type, const P2MRKeyMetadata& metadata)
{
    const P2MRLeafInfo& leaf = metadata.GetLeaf(spend_type);
    int w = 1; // witness stack count

    switch (spend_type) {
    case P2MRSpendType::SCHNORR:
        w += 1 + 64;                            // schnorr sig
        w += 1 + leaf.script.size();             // leaf script
        w += 1 + leaf.control_block.size();      // control block (65 bytes)
        break;
    case P2MRSpendType::SLH_DSA:
        w += 3 + 7857;                           // slh-dsa sig (varint len)
        w += 1 + leaf.script.size();
        w += 1 + leaf.control_block.size();      // control block (65 bytes)
        break;
    case P2MRSpendType::HYBRID:
        w += 3 + 7856;                           // slh-dsa sig
        w += 1 + 64;                             // schnorr sig
        w += 1 + leaf.script.size();
        w += 1 + leaf.control_block.size();      // control block (33 bytes)
        break;
    }
    return w;
}

// ============================================================================
// String Conversion
// ============================================================================

std::string P2MRScriptPubKeyMan::SpendTypeToString(P2MRSpendType type)
{
    switch (type) {
        case P2MRSpendType::SCHNORR:  return "schnorr";
        case P2MRSpendType::SLH_DSA:  return "slh-dsa";
        case P2MRSpendType::HYBRID:   return "hybrid";
        default: return "unknown";
    }
}

std::optional<P2MRSpendType> P2MRScriptPubKeyMan::StringToSpendType(const std::string& str)
{
    if (str == "schnorr")                    return P2MRSpendType::SCHNORR;
    if (str == "slh-dsa" || str == "slhdsa") return P2MRSpendType::SLH_DSA;
    if (str == "hybrid")                     return P2MRSpendType::HYBRID;
    return std::nullopt;
}

} // namespace wallet