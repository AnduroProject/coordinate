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

void RegisterMarachainRPCCommands(CRPCTable& t)
{
    static const CRPCCommand commands[]{
        {"marachain", &createAuxBlock},
        {"marachain", &submitAuxBlock},
        {"marachain", &getPendingDeposit},
    };
    for (const auto& c : commands) {
        t.appendCommand(c.name, &c);
    }
}