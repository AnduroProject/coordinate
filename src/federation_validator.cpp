
#include "federation_validator.h"
#include <string.h>
#include <script/standard.h>
#include <logging.h>
#include <outputtype.h>
#include <rpc/util.h>


float thresold = 60.0;


bool getRedeemPathHasEnoughThresold(std::vector<std::string> fullQuorum, std::vector<std::string> signaturePath)
{
    float matches = 0.0;
    int32_t totalQuorum = (float)fullQuorum.size();
    for (size_t i = 0; i < signaturePath.size(); i++) {
         for (size_t j = 0; i < fullQuorum.size(); j++) {
            if(fullQuorum[j].compare(signaturePath[i]) == 0) {
              matches = matches + 1.0;
              break;
            }
         }
    }
    float currentQuoram = (matches / totalQuorum) * 100.0;
    if(currentQuoram >= thresold) {
      return true;
    }
    return false;
}

std::string getCombinePubkeyForDerivationIndex(std::vector<std::string> xPubKeys, unsigned int derivationIndex) {
    secp256k1_context* ctx;
    ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    std::vector<CPubKey> trPubkeys;
    for (size_t i = 0; i < xPubKeys.size(); i++) {
        CPubKey keyItem = getPubKeyAtDerviationIndex(xPubKeys[i],derivationIndex);
        trPubkeys.push_back(keyItem);
    }
    return combinePublicKey(ctx, trPubkeys);
}

std::string getSchnorrNonce(std::string sessionIdIn, std::string messageIn, std::string xPrivKey, unsigned int derivationIndex) {
    secp256k1_context* ctx;
    ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);

    struct signerNonce signerNonceItem = generateSignerNonce(ctx,sessionIdIn,messageIn,xPrivKey,derivationIndex);
    unsigned char result[66];
    secp256k1_musig_pubnonce_serialize(ctx,result,&signerNonceItem.pubnonce);

    LogPrintf("log characters1 %c \n",result);

    unsigned char result1[66];
    memcpy(result1, result, 66);

    LogPrintf("log characters1 %c \n",result1);

    // std::string result;


    return "testing";
}

std::string getSchnorrNonceCombined(std::vector<std::string> nonces) {
    secp256k1_context* ctx;
    ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    const secp256k1_musig_pubnonce *pubnonces[nonces.size()];
    for (size_t i = 0; i < nonces.size(); i++) {
      unsigned char *pubNonceStr;
      std::copy( nonces[i].begin(), nonces[i].end(), pubNonceStr );
      secp256k1_musig_pubnonce pubnonce;
      secp256k1_musig_pubnonce_parse(ctx, &pubnonce, pubNonceStr);
      pubnonces[i] = &pubnonce;
    }

    secp256k1_musig_aggnonce agg_pubnonce;
    secp256k1_musig_nonce_agg(ctx, &agg_pubnonce, pubnonces, nonces.size());

    unsigned char result;
    secp256k1_musig_aggnonce_serialize(ctx,&result,&agg_pubnonce);
    
    std::string nonceResult((char*) result);
    return nonceResult;
}

std::string getSchnorrPartialSign(std::string aggNoncesIn, std::string sessionIdIn, std::string messageIn, std::string xPrivKey, unsigned int derivationIndex) {
    secp256k1_context* ctx;
    ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);

    secp256k1_musig_keyagg_cache* cache;
    secp256k1_musig_session session;
    secp256k1_musig_partial_sig partial_sig;

    secp256k1_musig_aggnonce agg_pubnonce;
    unsigned char *aggNoncesStr;
    std::copy(aggNoncesIn.begin(), aggNoncesIn.end(), aggNoncesStr);
    secp256k1_musig_aggnonce_parse(ctx, &agg_pubnonce, aggNoncesStr);


    secp256k1_keypair keypair = generateKeypair(ctx,xPrivKey,derivationIndex);
    
    struct signerNonce signerNonceItem = generateSignerNonce(ctx,sessionIdIn,messageIn,xPrivKey,derivationIndex);

    unsigned char *msg;
    std::copy(messageIn.begin(),messageIn.end(),msg);

    secp256k1_musig_nonce_process(ctx, &session, &agg_pubnonce, msg, cache, NULL);
    
    secp256k1_musig_partial_sign(ctx, &partial_sig, &signerNonceItem.secnonce, &keypair, cache, &session);

    unsigned char result;
    secp256k1_musig_partial_sig_serialize(ctx,&result,&partial_sig);
    
    std::string partialSigResult((char*) result);
    return partialSigResult;
}

std::string getSchnorrSignatureCombined(std::vector<std::string> signaturesIn, std::string aggNoncesIn, std::string messageIn) {
    secp256k1_context* ctx;
    ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);

    secp256k1_musig_session session;
    secp256k1_musig_keyagg_cache* cache;
    secp256k1_musig_aggnonce agg_pubnonce;

    unsigned char *msg;
    std::copy(messageIn.begin(),messageIn.end(),msg);

    unsigned char *aggNoncesStr;
    std::copy(aggNoncesIn.begin(), aggNoncesIn.end(), aggNoncesStr);
    secp256k1_musig_aggnonce_parse(ctx, &agg_pubnonce, aggNoncesStr);

    secp256k1_musig_nonce_process(ctx, &session, &agg_pubnonce, msg, cache, NULL);

    const secp256k1_musig_partial_sig *signatures[signaturesIn.size()];
    for (size_t i = 0; i < signaturesIn.size(); i++) {
      unsigned char *signatureStr;
      std::copy( signaturesIn[i].begin(), signaturesIn[i].end(), signatureStr );
      secp256k1_musig_partial_sig signature;
      secp256k1_musig_partial_sig_parse(ctx, &signature, signatureStr);
      signatures[i] = &signature;
    }

    unsigned char *sig64;

    secp256k1_musig_partial_sig_agg(ctx, sig64, &session, signatures, signaturesIn.size());

    std::string fullSignature((char*) sig64);

    return fullSignature;
}

bool verifyFederationSchnorrSignature(std::vector<std::string> fullQuorum, std::vector<std::string> signaturePath, std::string aggPubIn, std::string signatureIn, std::string messageIn) {
    if(!getRedeemPathHasEnoughThresold(fullQuorum, signaturePath)) {
       return false;
    }

    secp256k1_context* ctx;
    ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);

    std::vector<CPubKey> trPubkeys;

    for (size_t i = 0; i < signaturePath.size(); i++) {
        CPubKey pubObj(HexToPubKey(signaturePath[i]));
        trPubkeys.push_back(pubObj);
    }
    std::string currentRedeemKey = combinePublicKey(ctx, trPubkeys);
    if(currentRedeemKey.compare(aggPubIn) != 0) {
        return false;
    }

    unsigned char *msg;
    std::copy(messageIn.begin(),messageIn.end(),msg);

    unsigned char *sig;
    std::copy(signatureIn.begin(),signatureIn.end(),sig);

    unsigned char *aggPubStr;
    std::copy(aggPubIn.begin(),aggPubIn.end(),aggPubStr);

    secp256k1_xonly_pubkey aggpk;
    secp256k1_xonly_pubkey_parse(ctx,&aggpk,aggPubStr);
    
    
    if (!secp256k1_schnorrsig_verify(ctx, sig, msg, sizeof(msg), &aggpk)) {
        return false;
    }
    return true;
}

CPubKey getPubKeyAtDerviationIndex(std::string xPubKey, unsigned int derivationIndex) {
    CExtPubKey pubkey = DecodeExtPubKey(xPubKey);
    CExtPubKey pubkey1;
    pubkey.Derive(pubkey1,0);

    CExtPubKey pubkey2;
    pubkey1.Derive(pubkey2,derivationIndex);

    return pubkey2.pubkey;
}

CKey getKeyAtDerviationIndex(std::string xPrivKey, unsigned int derivationIndex) {
    CExtKey key = DecodeExtKey(xPrivKey);
    CExtPubKey pubkey;
    pubkey = key.Neuter();

    CExtKey key1;
    key.Derive(key1, 0);

    CExtKey key2;
    key1.Derive(key2, derivationIndex);

    return key2.key;
}

secp256k1_keypair generateKeypair(const secp256k1_context* ctx, std::string xPrivKey, unsigned int derivationIndex) {
    CKey key = getKeyAtDerviationIndex(xPrivKey,derivationIndex);
    secp256k1_keypair keypair;
    secp256k1_keypair_create(ctx, &keypair, key.begin());
    return keypair;
}

signerNonce generateSignerNonce(const secp256k1_context* ctx, std::string sessionIdIn, std::string messageIn, std::string xPrivKey, unsigned int derivationIndex) {

    struct signerNonce signerNonceItem;
    // unsigned char session_id[sessionIdIn.length()];
    // std::strcpy((char*)session_id, sessionIdIn.c_str());
    // unsigned char msg[messageIn.length()];
    // std::strcpy((char*)msg, messageIn.c_str());

    unsigned char session_id[] = "test";
    unsigned char msg[] = "test";

    // LogPrintf("session_id %c \n", session_id);
    // LogPrintf("msg %c \n", msg);
    // std::copy( sessionIdIn.begin(), sessionIdIn.end(), session_id );
    // std::copy( messageIn.begin(), messageIn.end(), msg );

    CKey key = getKeyAtDerviationIndex(xPrivKey,derivationIndex);

    secp256k1_keypair keypair = generateKeypair(ctx,xPrivKey,derivationIndex);

    secp256k1_pubkey pubkey;
    secp256k1_keypair_pub(ctx, &pubkey, &keypair);


    secp256k1_musig_secnonce secnonce;
    secp256k1_musig_pubnonce pubnonce;

    secp256k1_musig_nonce_gen(ctx, &signerNonceItem.secnonce, &signerNonceItem.pubnonce, session_id, key.begin(), &pubkey, msg, NULL, NULL);

    return signerNonceItem;
}

std::string combinePublicKey(const secp256k1_context* ctx, std::vector<CPubKey> pubKeys) {
    const secp256k1_pubkey *pubkeys_ptr[pubKeys.size()];
    secp256k1_xonly_pubkey agg_pk;
    secp256k1_musig_keyagg_cache cache;

    for (size_t i = 0; i < pubKeys.size(); i++)
    {
       secp256k1_pubkey pk;
       if (secp256k1_ec_pubkey_parse(ctx, &pk, pubKeys[i].data(),pubKeys[i].size())) {
           pubkeys_ptr[i] = &pk;
       }
    }

    if (!secp256k1_musig_pubkey_agg(ctx, NULL, &agg_pk, &cache, pubkeys_ptr, pubKeys.size())) {
        return "";
    }
    
    unsigned char tweak[32];
    secp256k1_xonly_pubkey_serialize(ctx, tweak, &agg_pk);

    return HexStr(XOnlyPubKey(tweak));
}
