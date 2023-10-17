

#include <iostream>
#include <util/strencodings.h>
#include "pubkey.h"
#include <secp256k1_schnorrsig.h>
#include <key_io.h>


bool getRedeemPathAvailable(std::vector<std::string> fullQuorum, std::string signaturePath);
uint256 prepareMessageHash(std::string message);
bool validateFederationSignature(std::string witnessHex, std::string message, std::string prevWitnessHex);