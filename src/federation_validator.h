

#include <iostream>
#include <util/strencodings.h>
#include "pubkey.h"
#include <secp256k1_schnorrsig.h>
#include <secp256k1_musig.h>
#include <key_io.h>

struct signerNonce {
    secp256k1_musig_secnonce secnonce;
    secp256k1_musig_pubnonce pubnonce;
};

bool getRedeemPathHasEnoughThresold(std::vector<std::string> fullQuorum, std::vector<std::string> signaturePath);
std::string getCombinePubkeyForDerivationIndex(std::vector<std::string> xPubKeys, unsigned int derivationIndex);
std::string getSchnorrNonce(std::string sessionIdIn, std::string messageIn, std::string xPrivKey, unsigned int derivationIndex);
std::string getSchnorrNonceCombined(std::vector<std::string> nonces);
std::string getSchnorrPartialSign(std::string aggNoncesIn, std::string sessionIdIn, std::string messageIn, std::string xPrivKey, unsigned int derivationIndex);
std::string getSchnorrSignatureCombined(std::vector<std::string> signaturesIn, std::string aggNoncesIn, std::string messageIn);
bool verifyFederationSchnorrSignature(std::vector<std::string> fullQuorum, std::vector<std::string> signaturePath, std::string aggPubIn, std::string signatureIn, std::string messageIn);
CPubKey getPubKeyAtDerviationIndex(std::string xPubKey, unsigned int derivationIndex);
CKey getKeyAtDerviationIndex(std::string xPrivKey, unsigned int derivationIndex);
secp256k1_keypair generateKeypair(const secp256k1_context* ctx, std::string xPrivKey, unsigned int derivationIndex);
signerNonce generateSignerNonce(const secp256k1_context* ctx, std::string sessionIdIn, std::string messageIn, std::string xPrivKey, unsigned int derivationIndex);
std::string combinePublicKey(const secp256k1_context* ctx, std::vector<CPubKey> pubKeys);
