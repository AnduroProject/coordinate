#include <iostream>
#include <util/strencodings.h>
#include "pubkey.h"
#include <secp256k1_schnorrsig.h>
#include <key_io.h>

/**
 * Check the witness signature path available in authorized Anduro keys
 * @param[in] fullQuorum authorized Anduro full path
 * @param[in] signaturePath path signed by the Anduro for presigned block
 */
bool getRedeemPathAvailable(std::vector<std::string> fullQuorum, std::string signaturePath);

/**
 * Check used to prepare sha256 hash for presigned block message
 * @param[in] message presigned block message
 */
uint256 prepareMessageHash(std::string message);
[
/**
 * Validate presigned signature
 * @param[in] witnessHex block witness which hold signature path and signature
 * @param[in] message sha256 presigned block message
 * @param[in] prevWitnessHex Anduro current keys
*/
bool validateAnduroSignature(std::string witnessHex, std::string message, std::string prevWitnessHex);

/**
 * This function used to validate presigned signature
 * @param[in] signatureHex block witness which hold signature path and signature
 * @param[in] messageIn sha256 presigned block message
 * @param[in] prevWitnessHex Anduro current keys
*/
bool validatePreconfSignature(std::string signatureHex, std::string messageIn, std::string prevWitnessHex);
