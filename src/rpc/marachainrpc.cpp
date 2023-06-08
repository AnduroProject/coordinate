#include <rpc/marachainrpc.h>
#include <rpc/util.h>
#include <rpc/server.h>
#include <rpc/server.h>
#include <rpc/server_util.h>
#include <key_io.h>
#include <rpc/auxpow_miner.h>
#include <core_io.h>
#include <federation_deposit.h>

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
            {"block", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "It is the serialized string of the auxiliary proof of work."},
            {"prevblock", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "It is the serialized string of the auxiliary proof of work."},
            {"merkle", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "It is the serialized string of the auxiliary proof of work."},
            {"nVersion", RPCArg::Type::STR, RPCArg::Optional::NO, "nVersion"},
            {"nTime", RPCArg::Type::STR, RPCArg::Optional::NO, "nTime"},
            {"nBits", RPCArg::Type::STR, RPCArg::Optional::NO, "nBits"},
            {"nNonce", RPCArg::Type::STR, RPCArg::Optional::NO, "nNonce"},
            {"coinbase", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "Coinbase"},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::BOOL, "result", /*optional=*/true, "Only returns true if submit completed successfully"},
            },
        },
        RPCExamples{
            HelpExampleCli("submitauxblock", "\"AuxBlockHash\" \"ParentBlockHash\" \"ParentPrevBlockHash\" \"ParentMerkleRoot\" nVersion nTime nBits nNonce \"coinbaseRawTx\" ")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            return AuxpowMiner::get ().submitAuxBlock(request,
                                                    request.params[0].get_str(),
                                                    request.params[1].get_str(),
                                                    request.params[2].get_str(),
                                                    request.params[3].get_str(),
                                                    request.params[4].get_str(),
                                                    request.params[5].get_str(),
                                                    request.params[6].get_str(),
                                                    request.params[7].get_str(),
                                                    request.params[8].get_str());
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
                            {RPCResult::Type::ARR, "vout", "",
                            {
                                {RPCResult::Type::OBJ, "", "",
                                {
                                    {RPCResult::Type::STR_AMOUNT, "value", "The value in " + CURRENCY_UNIT},
                                    {RPCResult::Type::NUM, "n", "index"},
                                }},
                            }},
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
                result.pushKV("total", ValueFromAmount(listPendingDepositTotal()));

                
                UniValue deposits(UniValue::VARR);
                std::vector<CTxOut> txList = listPendingDepositTransaction();
 
                UniValue txresult(UniValue::VOBJ);
                UniValue vout(UniValue::VARR);
                uint64_t index = 0;
                for (const CTxOut& eout : txList) {
                    UniValue toutresult(UniValue::VOBJ);
                    toutresult.pushKV("index", index);
                    toutresult.pushKV("value", ValueFromAmount(eout.nValue));
                    index = index + 1;
                    vout.push_back(toutresult);
                }
                txresult.pushKV("vout", vout);
                deposits.push_back(txresult);
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


static RPCHelpMan sendpegtransaction()
{

        return RPCHelpMan{
            "sendpegtransaction",
            "submit tx out from federation",
        CreateTxDoc(),
        RPCResult{
            RPCResult::Type::STR, "", "The transaction hash in hex"
        },
        RPCExamples{
           HelpExampleCli("sendpegtransaction", "")
           + HelpExampleRpc("sendpegtransaction", "\"[{\\\"address\\\":\\\"myid\\\",\\\"amount\\\":0}]\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            if (request.params[0].isNull()) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Transaction out are empty");
                return true;
            }
            const UniValue& output_params = request.params[0].get_array();
            for (unsigned int idx = 0; idx < output_params.size(); idx++) {
                const UniValue& o = output_params[idx].get_obj();
                RPCTypeCheckObj(o,
                {
                    {"address", UniValueType(UniValue::VSTR)},
                    {"amount", UniValueType(UniValue::VSTR)},
                });
                 const CTxDestination coinbaseScript = DecodeDestination( find_value(o, "address").get_str());
                 const CScript scriptPubKey = GetScriptForDestination(coinbaseScript);
                 CTxOut out(AmountFromValue(find_value(o, "amount")), scriptPubKey);
                 addDeposit(out);
            }
            return "success";
        }
    };

}


void RegisterMarachainRPCCommands(CRPCTable& t)
{
    static const CRPCCommand commands[]{
        {"marachain", &createAuxBlock},
        {"marachain", &submitAuxBlock},
        {"marachain", &getPendingDeposit},
        {"marachain",&sendpegtransaction}
    };
    for (const auto& c : commands) {
        t.appendCommand(c.name, &c);
    }
}
