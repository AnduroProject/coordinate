#include <anduro_validator.h>
#include <string.h>
#include <logging.h>
#include <outputtype.h>
#include <rpc/util.h>
#include <cmath>
/**
 * Validate presigned signature
 */
bool validateAnduroSignature(std::string signatureHex, std::string messageIn, std::string prevWitnessHex) {
    std::vector<unsigned char> wData(ParseHex(prevWitnessHex));
    const std::string prevWitnessHexStr(wData.begin(), wData.end());
    UniValue witnessVal(UniValue::VOBJ);
    if (!witnessVal.read(prevWitnessHexStr)) {
        LogPrintf("invalid previous currentkeys params \n");
        return false;
    }

    LogPrintf("witness received %s \n", signatureHex);
    std::vector<unsigned char> sData(ParseHex(signatureHex));
    const std::string signatureHexStr(sData.begin(), sData.end());
    UniValue witness(UniValue::VOBJ);
    if (!witness.read(signatureHexStr)) {
        LogPrintf("invalid witness received 123 \n");
        return false;
    }

    std::string signature = witness.get_obj().find_value("signature").get_str();
    std::string pubkey =  witness.get_obj().find_value("redeempath").get_str();

    uint256 message = prepareMessageHash(messageIn);
    LogPrintf("validateAnduroSignature message %s \n", message.ToString());
    LogPrintf("validateAnduroSignature pubkey %s \n", pubkey);
    LogPrintf("validateAnduroSignature signature %s \n", signature);
    XOnlyPubKey xPubkey(CPubKey(ParseHex(pubkey)));
    auto tweaked = xPubkey.CreateTapTweak(nullptr);
    XOnlyPubKey tweaked_key = tweaked->first;
    if(!tweaked_key.VerifySchnorr(message,ParseHex(signature))) {
        LogPrintf("failed verfication \n");
        return false;
    } 
    return true;
}

bool validatePreconfSignature(std::string signatureHex, std::string messageIn, std::string prevWitnessHex) {
    std::vector<unsigned char> wData(ParseHex(prevWitnessHex));
    const std::string prevWitnessHexStr(wData.begin(), wData.end());
    UniValue witnessVal(UniValue::VOBJ);
    if (!witnessVal.read(prevWitnessHexStr)) {
        LogPrintf("validatePreconfSignature: invalid witness params \n");
        return false;
    }

    std::vector<std::string> allKeysArray;
    const auto allKeysArrayRequest = witnessVal.get_obj().find_value("all_keys").get_array();
    for (size_t i = 0; i < allKeysArrayRequest.size(); i++) {
        allKeysArray.push_back(allKeysArrayRequest[i].get_str());
    }

    int thresold = 1;
   
    std::vector<unsigned char> sData(ParseHex(signatureHex));
    const std::string signatureHexStr(sData.begin(), sData.end());
    UniValue allSignatures(UniValue::VOBJ);
    if (!allSignatures.read(signatureHexStr)) {
        LogPrintf("validatePreconfSignature: invalid signature params \n");
        return false;
    }

    std::string signature = allSignatures.get_obj().find_value("signature").get_str();
    std::string pubkey =  allSignatures.get_obj().find_value("redeempath").get_str();
    LogPrintf("validatePreconfSignature: pubkey %s \n", pubkey);

    LogPrintf("all keys \n");
    for (size_t i = 0; i < allKeysArray.size(); i++) {
        LogPrintf("allKeysArray[i] %s \n", allKeysArray[i]);
    }
    LogPrintf("all keys end \n");
    if(getRedeemPathAvailable(allKeysArray,pubkey)) {
        uint256 message = prepareMessageHash(messageIn);
        LogPrintf("validatePreconfSignature message %s \n", message.ToString());
        LogPrintf("validatePreconfSignature pubkey %s \n", pubkey);
        LogPrintf("validatePreconfSignature signature %s \n", signature);
        XOnlyPubKey xPubkey(CPubKey(ParseHex(pubkey)));
        if(!xPubkey.VerifySchnorr(message,ParseHex(signature))) {
            LogPrintf("validatePreconfSignature: failed verfication \n");
        } else {
            return true;
        }
    } else {
        LogPrintf("validatePreconfSignature: key not available \n");
    }
    return false;
}

/**
 * Prepare sha256 hash for presigned block message
 */
uint256 prepareMessageHash(std::string message) {
    uint256 messageBuffer;
    CSHA256().Write((unsigned char*)message.data(), message.size()).Finalize(messageBuffer.begin());
    return messageBuffer;
//     std::vector<unsigned char> vchRootHash(messageBuffer.begin(), messageBuffer.end());
//     std::reverse(vchRootHash.begin(), vchRootHash.end());
//     return uint256(vchRootHash);
}

/**
 * Signature path available in authorized anduro keys
 */
bool getRedeemPathAvailable(std::vector<std::string> fullQuorum, std::string signaturePath) {
    bool isSignaturePathExist = false;
    for (size_t i = 0; i < fullQuorum.size(); i++) {
        if(fullQuorum[i].compare(signaturePath) == 0) {
            isSignaturePathExist = true;
            break;
        }
    }

    return isSignaturePathExist;
}
