
#include "federation_validator.h"
#include <string.h>
#include <script/standard.h>
#include <logging.h>
#include <outputtype.h>
#include <rpc/util.h>

bool validateFederationSignature(std::string signatureHex, std::string messageIn, std::string prevWitnessHex) {
   std::vector<unsigned char> sData(ParseHex(signatureHex));
   const std::string signatureHexStr(sData.begin(), sData.end());
   UniValue val(UniValue::VOBJ);
   if (!val.read(signatureHexStr)) {
      LogPrintf("invalid signature params \n");
      return false;
   }

   std::vector<unsigned char> wData(ParseHex(prevWitnessHex));
   const std::string prevWitnessHexStr(wData.begin(), wData.end());
   UniValue witnessVal(UniValue::VOBJ);
   if (!witnessVal.read(prevWitnessHexStr)) {
      LogPrintf("invalid witness params \n");
      return false;
   }

   std::vector<std::string> allKeysArray;
   const auto allKeysArrayRequest = find_value(witnessVal.get_obj(), "allkeys").get_array();
   for (size_t i = 0; i < allKeysArrayRequest.size(); i++) {
        allKeysArray.push_back(allKeysArrayRequest[i].get_str());
   }
   
   std::string redeemPath =  find_value(val.get_obj(), "redeemkeys").get_str();
   std::string signature =  find_value(val.get_obj(), "signature").get_str();

   uint256 message = prepareMessageHash(messageIn);

    if(!XOnlyPubKey(ParseHex(redeemPath)).VerifySchnorr(message,ParseHex(signature))) {
       return false;
    } 
    return true;

}

uint256 prepareMessageHash(std::string message) {
    uint256 messageBuffer;
    CSHA256().Write((unsigned char*)message.data(), message.size()).Finalize(messageBuffer.begin());
    return messageBuffer;
}



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
   