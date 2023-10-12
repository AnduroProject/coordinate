#include <rpc/marachainrpc.h>
#include <rpc/util.h>
#include <rpc/server.h>
#include <rpc/server.h>
#include <rpc/server_util.h>
#include <key_io.h>
#include <rpc/auxpow_miner.h>
#include <core_io.h>
#include <federation_deposit.h>
#include <federation_validator.h>

static RPCHelpMan createAuxBlock()
{
    return RPCHelpMan{
        "createauxblock",
        "The mining pool to request new marachain block header hashes",
        {
            {"paytoaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "The marachain address that the mining pool wants to send the Babylon mining rewards to. Will be written to marachain coinbase transaction."},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR, "bits", "The target difficulty of marachain. The larger the bits, the higher the difficulty. Target hash in int = 2^256 / int(bits)."},
                {RPCResult::Type::NUM, "chainid", "marachain identity"},
                {RPCResult::Type::NUM, "height", "Height of transactions for the block"},
                {RPCResult::Type::NUM, "coinbasevalue", "Miner reward for the block"},
                {RPCResult::Type::STR_HEX, "hash", "The header hash of the created marachain block"},
                {RPCResult::Type::STR_HEX, "previousblockhash", "The previous hash of the created marachain block"},
          
            },
        },
        RPCExamples{
            HelpExampleCli("createauxblock", "90869d013db27608c7428251c6755e5a1d9e9313") +
            HelpExampleRpc("createauxblock", "\"90869d013db27608c7428251c6755e5a1d9e9313\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            const CTxDestination coinbaseScript = DecodeDestination(request.params[0].get_str());
            if (!IsValidDestination(coinbaseScript)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,"Error: Invalid coinbase payout address");
            }
            const CScript scriptPubKey = GetScriptForDestination(coinbaseScript);
            return AuxpowMiner::get ().createAuxBlock(request, scriptPubKey);
        }
    };
}

static RPCHelpMan submitAuxBlock()
{
    return RPCHelpMan{
        "submitauxblock",
        "The mining pool to submit the mined auxPoW",
        {
            {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "The marachain bloch header hash that the submitted auxPoW is associated with."},
            {"auxpow", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "It is the serialized string of the auxiliary proof of work."},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::BOOL, "result", /*optional=*/true, "Only returns true if submit completed successfully"},
            },
        },
        RPCExamples{
            HelpExampleCli("submitauxblock", "\"7926398947f332fe534b15c628ff0cd9dc6f7d3ea59c74801dc758ac65428e64\" \"02000000010000000000000000000000000000000000000000000000000000000000000000ffffffff4b0313ee0904a880495b742f4254432e434f4d2ffabe6d6d9581ba0156314f1e92fd03430c6e4428a32bb3f1b9dc627102498e5cfbf26261020000004204cb9a010f32a00601000000000000ffffffff0200000000000000001976a914c0174e89bd93eacd1d5a1af4ba1802d412afc08688ac0000000000000000266a24aa21a9ede2f61c3f71d1defd3fa999dfa36953755c690689799962b48bebd836974e8cf90000000014acac4ee8fdd8ca7e0b587b35fce8c996c70aefdf24c333038bdba7af531266000000000001ccc205f0e1cb435f50cc2f63edd53186b414fcb22b719da8c59eab066cf30bdb0000000000000020d1061d1e456cae488c063838b64c4911ce256549afadfc6a4736643359141b01551e4d94f9e8b6b03eec92bb6de1e478a0e913e5f733f5884857a7c2b965f53ca880495bffff7f20a880495b\" ")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            return AuxpowMiner::get ().submitAuxBlock(request,
                                                    request.params[0].get_str(),
                                                    request.params[1].get_str());
        }
    };

}

static RPCHelpMan hasPegOut()
{
    return RPCHelpMan{
        "haspegout",
        "check peg out already done",
        {},
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::BOOL, "result", /*optional=*/true, "Only returns true if submit completed successfully"},
            },
        },
        RPCExamples{
            HelpExampleCli("haspegout", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            std::vector<FederationTxOut> txList = listPendingDepositTransaction(-1);
            bool hasPegout = false;
            for (const FederationTxOut& eout : txList) {
                if(eout.pegInfo.compare("") != 0) {
                   hasPegout = true;
                }
            }
            return hasPegout;
        }
    };

}

static RPCHelpMan federationDepositAddress()
{
    return RPCHelpMan{
        "federationdepositaddress",
        "Federation current deposit address",
        {},
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR, "result", /*optional=*/true, "Returns federation deposit address"},
            },
        },
        RPCExamples{
            HelpExampleCli("federationdepositaddress", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            return getDepositAddress();
        }
    };

}

static RPCHelpMan federationWithdrawAddress()
{
    return RPCHelpMan{
        "federationburnaddress",
        "Federation current burn address",
        {},
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR, "result", /*optional=*/true, "Returns federation burn address"},
            },
        },
        RPCExamples{
            HelpExampleCli("federationburnaddress", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            return getBurnAddress();
        }
    };

}

static RPCHelpMan getPendingDeposit() {
        return RPCHelpMan{
        "getpendingdeposit",
        "get all pending depsoits",
        {
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::ARR, "deposits", "",
                {
                     {RPCResult::Type::OBJ, "", "",
                        {
                            {RPCResult::Type::STR_AMOUNT, "value", "The value in " + CURRENCY_UNIT},
                            {RPCResult::Type::NUM, "n", "index"},
                            {RPCResult::Type::NUM, "n", "block_height"},
                        }
                     }
                }},
                {RPCResult::Type::STR_AMOUNT, "total", /*optional=*/true, "return total pending balance"},
            },
        },
        RPCExamples{
           HelpExampleCli("getpendingdeposit", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
                UniValue result(UniValue::VOBJ);
                result.pushKV("total", ValueFromAmount(listPendingDepositTotal(-1)));
                UniValue deposits(UniValue::VARR);
                std::vector<FederationTxOut> txList = listPendingDepositTransaction(-1);
                uint64_t index = 0;
                for (const FederationTxOut& eout : txList) {
                    UniValue toutresult(UniValue::VOBJ);
                    toutresult.pushKV("index", index);
                    toutresult.pushKV("value", ValueFromAmount(eout.nValue));
                    toutresult.pushKV("block_height", eout.block_height);
                    index = index + 1;
                    deposits.push_back(toutresult);
                }
                result.pushKV("deposits", deposits);
                return result;
        }
    };

}

static std::vector<RPCArg> CreateTxDoc()
{
    return {
        {"inputs", RPCArg::Type::ARR, RPCArg::Optional::NO, "The inputs",
            {
                {"", RPCArg::Type::OBJ, RPCArg::Optional::OMITTED, "",
                    {
                        {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "The bitcoin address"},
                        {"amount", RPCArg::Type::NUM, RPCArg::Optional::NO, "The bitcoin amount"}
                    },
                },
            },
        }
    };
}

static RPCHelpMan getFederationPubkey()
{
    return RPCHelpMan{
        "getfederationpubkey",
        "get pubkey combined for the federation",
        {
            {"xpubkey", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "federation root public keys"},
            {"derivationindex", RPCArg::Type::NUM, RPCArg::Optional::NO, "current derviation index of xpub"}
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR, "result", /*optional=*/true, "Returns federation pub key"},
            },
        },
        RPCExamples{
            HelpExampleCli("getfederationpubkey", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            std::string xPubStr = request.params[0].get_str();
            const unsigned int derivationIndex = request.params[1].getInt<unsigned int>();
            return HexStr(getPubKeyAtDerviationIndex(xPubStr, derivationIndex));
        }
    };
}

static RPCHelpMan getFederationCombinedPubkey()
{
    return RPCHelpMan{
        "getfederationcombinedpubkey",
        "get pubkey combined for the federation",
        {
            {"xpubkeys", RPCArg::Type::ARR, RPCArg::Optional::NO, "An array for all federation root public keys",
                {
                    {"xpubkey", RPCArg::Type::STR_HEX, RPCArg::Optional::OMITTED, ""},
                },
            },
            {"derivationindex", RPCArg::Type::NUM, RPCArg::Optional::NO, "current derviation index of xpub"}
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR, "result", /*optional=*/true, "Returns federation combined pub key"},
            },
        },
        RPCExamples{
            HelpExampleCli("getfederationcombinedpubkey", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            std::vector<std::string> xPubStr;
            const unsigned int derivationIndex = request.params[1].getInt<unsigned int>();
            const auto xPubStrRequest = request.params[0].get_array();
            for (size_t i = 0; i < xPubStrRequest.size(); i++) {
               xPubStr.push_back(xPubStrRequest[i].get_str());
            }
            return getCombinePubkeyForDerivationIndex(xPubStr, derivationIndex);
        }
    };
}

static RPCHelpMan getFederationGenerateNonce()
{
    return RPCHelpMan{
        "getfederationgeneratenonce",
        "get pubkey combined for the federation",
        {
            {"xprivkey", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "federation root private key"},
            {"derivationindex", RPCArg::Type::NUM, RPCArg::Optional::NO, "current derviation index of xpub"},
            {"message", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "federation message to generate nonce"},
            {"session", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "federation session for current nonce"},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR, "result", /*optional=*/true, "Returns federation pub key"},
            },
        },
        RPCExamples{
            HelpExampleCli("getfederationgeneratenonce", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            const unsigned int derivationIndex = request.params[1].getInt<unsigned int>();
            return getSchnorrNonce(request.params[3].get_str(), request.params[2].get_str(), request.params[0].get_str(), derivationIndex);
        }
    };
}

static RPCHelpMan getFederationCombinedNonces()
{
    return RPCHelpMan{
        "getfederationcombinednonce",
        "get pubkey combined for the federation",
        {
            {"nonces", RPCArg::Type::ARR, RPCArg::Optional::NO, "An array for all federation root nonce for partial signature",
                {
                    {"nonce", RPCArg::Type::STR_HEX, RPCArg::Optional::OMITTED, ""},
                },
            },
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR, "result", /*optional=*/true, "Returns federation combined pub key"},
            },
        },
        RPCExamples{
            HelpExampleCli("getfederationcombinednonce", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            std::vector<std::string> noncesStr;
            const auto noncesStrRequest = request.params[0].get_array();
            for (size_t i = 0; i < noncesStrRequest.size(); i++) {
               noncesStr.push_back(noncesStrRequest[i].get_str());
            }
            return getSchnorrNonceCombined(noncesStr);
        }
    };
}

static RPCHelpMan getFederationGeneratePartialSign()
{
    return RPCHelpMan{
        "getfederationgeneratepartialsign",
        "generate partial signature for federation node",
        {    
            {"xprivkey", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "federation root private key"},
            {"derivationindex", RPCArg::Type::NUM, RPCArg::Optional::NO, "current derviation index of xpub"},
            {"message", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "federation message to generate nonce"},
            {"session", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "federation session for current nonce"},
            {"nonce", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "federation combined nonce"},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR, "result", /*optional=*/true, "Returns federation partial signature"},
            },
        },
        RPCExamples{
            HelpExampleCli("getfederationgeneratepartialsign", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            const unsigned int derivationIndex = request.params[1].getInt<unsigned int>();
            return getSchnorrPartialSign(request.params[4].get_str(), request.params[3].get_str(), request.params[2].get_str(), request.params[0].get_str(), derivationIndex);
        }
    };
}

static RPCHelpMan getFederationCombinedSignatures()
{
    return RPCHelpMan{
        "getfederationcombinedsignatures",
        "combine signature for the federation",
        {
            {"signatures", RPCArg::Type::ARR, RPCArg::Optional::NO, "An array for all federation signature",
                {
                    {"signature", RPCArg::Type::STR_HEX, RPCArg::Optional::OMITTED, ""},
                },
            },
            {"message", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "federation message to generate nonce"},
            {"nonce", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "federation combined nonce"},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR, "result", /*optional=*/true, "Returns federation combined signature"},
            },
        },
        RPCExamples{
            HelpExampleCli("getfederationcombinedsignatures", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            std::vector<std::string> signaturesStr;
            const auto signaturesStrRequest = request.params[0].get_array();
            for (size_t i = 0; i < signaturesStrRequest.size(); i++) {
               signaturesStr.push_back(signaturesStrRequest[i].get_str());
            }
            return getSchnorrSignatureCombined(signaturesStr, request.params[2].get_str(), request.params[1].get_str());
        }
    };
}

static RPCHelpMan getFederationVerifySignature()
{
    return RPCHelpMan{
        "getfederationverifysignature",
        "verify signature for the federation",
        {
            {"allkeys", RPCArg::Type::ARR, RPCArg::Optional::NO, "An array for all federation pub keys",
                {
                    {"pubkey", RPCArg::Type::STR_HEX, RPCArg::Optional::OMITTED, ""},
                },
            },
            {"redeemkeys", RPCArg::Type::ARR, RPCArg::Optional::NO, "An array for all federation redeem pub keys",
                {
                    {"pubkey", RPCArg::Type::STR_HEX, RPCArg::Optional::OMITTED, ""},
                },
            },
            {"signature", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "federation full signature to be verified"},
            {"message", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "federation message to be verified"},
            {"aggkey", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "federation aggregated pub key"},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::BOOL, "result", /*optional=*/true, "Returns federation signature valid or not"},
            },
        },
        RPCExamples{
            HelpExampleCli("getfederationverifysignature", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            std::vector<std::string> allKeysArray;
            const auto allKeysArrayRequest = request.params[0].get_array();
            for (size_t i = 0; i < allKeysArrayRequest.size(); i++) {
               allKeysArray.push_back(allKeysArrayRequest[i].get_str());
            }

            std::vector<std::string> redeemKeysArray;
            const auto redeemKeysArrayRequest = request.params[1].get_array();
            for (size_t i = 0; i < redeemKeysArrayRequest.size(); i++) {
               redeemKeysArray.push_back(redeemKeysArrayRequest[i].get_str());
            }

            return verifyFederationSchnorrSignature(allKeysArray, redeemKeysArray, request.params[4].get_str(), request.params[2].get_str(), request.params[3].get_str());
        }
    };
}

void RegisterMarachainRPCCommands(CRPCTable& t)
{
    static const CRPCCommand commands[]{
        {"marachain", &createAuxBlock},
        {"marachain", &submitAuxBlock},
        {"marachain", &getPendingDeposit},
        {"marachain", &hasPegOut},
        {"marachain", &federationDepositAddress},
        {"marachain", &federationWithdrawAddress},
        {"marachain", &getFederationPubkey},
        {"marachain", &getFederationCombinedPubkey},
        {"marachain", &getFederationGenerateNonce},
        {"marachain", &getFederationCombinedNonces},
        {"marachain", &getFederationGeneratePartialSign},
        {"marachain", &getFederationCombinedSignatures},
        {"marachain", &getFederationVerifySignature},
    };
    for (const auto& c : commands) {
        t.appendCommand(c.name, &c);
    }
}


