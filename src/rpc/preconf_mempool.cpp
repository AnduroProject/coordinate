
#include <rpc/blockchain.h>
#include <kernel/mempool_persist.h>
#include <chainparams.h>
#include <core_io.h>
#include <fs.h>
#include <kernel/mempool_entry.h>
#include <node/mempool_persist_args.h>
#include <policy/rbf.h>
#include <policy/settings.h>
#include <primitives/transaction.h>
#include <rpc/server.h>
#include <rpc/server_util.h>
#include <rpc/util.h>
#include <txmempool.h>
#include <univalue.h>
#include <util/moneystr.h>
#include <util/time.h>
#include <utility>
#include <anduro_deposit.h>
#include <anduro_validator.h>
#include <coordinate/coordinate_preconf.h>
#include <node/blockstorage.h>

using node::DEFAULT_MAX_RAW_TX_FEE_RATE;
using node::NodeContext;
using node::BlockManager;
using node::ReadBlockFromDisk;


static RPCHelpMan sendpreconftransaction()
{
    return RPCHelpMan{"sendpreconftransaction",
        "\nSubmit a raw transaction (serialized, hex-encoded) to local node and network.\n"
        "\nThe transaction will be sent unconditionally to all peers, so using sendpreconftransaction\n"
        "for manual rebroadcast may degrade privacy by leaking the transaction's origin, as\n"
        "nodes will normally not rebroadcast non-wallet transactions already in their mempool.\n"
        "\nA specific exception, RPC_TRANSACTION_ALREADY_IN_CHAIN, may throw if the transaction cannot be added to the mempool.\n"
        "\nRelated RPCs: createrawtransaction, signrawtransactionwithkey\n",
        {
            {"hexstring", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "The hex string of the raw transaction"},
            {"maxfeerate", RPCArg::Type::AMOUNT, RPCArg::Default{FormatMoney(DEFAULT_MAX_RAW_TX_FEE_RATE.GetFeePerK())},
             "Reject transactions whose fee rate is higher than the specified value, expressed in " + CURRENCY_UNIT +
                 "/kvB.\nSet to 0 to accept any fee rate.\n"},
            {"assethexstring", RPCArg::Type::STR_HEX, RPCArg::Optional::OMITTED, "The hex string of the asset data"},
        },
        RPCResult{
            RPCResult::Type::STR_HEX, "", "The transaction hash in hex"
        },
        RPCExamples{
            "\nCreate a transaction\n"
            + HelpExampleCli("createrawtransaction", "\"[{\\\"txid\\\" : \\\"mytxid\\\",\\\"vout\\\":0}]\" \"{\\\"myaddress\\\":0.01}\"") +
            "Sign the transaction, and get back the hex\n"
            + HelpExampleCli("signrawtransactionwithwallet", "\"myhex\"") +
            "\nSend the transaction (signed hex)\n"
            + HelpExampleCli("sendrawtransaction", "\"signedhex\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("sendrawtransaction", "\"signedhex\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {

            CMutableTransaction mtx;
            if (!DecodeHexTx(mtx, request.params[0].get_str())) {
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed. Make sure the tx has at least one input.");
            }
            CTransactionRef tx(MakeTransactionRef(std::move(mtx)));
            const CFeeRate max_raw_tx_fee_rate = request.params[1].isNull() ?
                                                    DEFAULT_MAX_RAW_TX_FEE_RATE :
                                                    CFeeRate(AmountFromValue(request.params[1]));

            int64_t virtual_size = GetVirtualTransactionSize(*tx);
            CAmount max_raw_tx_fee = max_raw_tx_fee_rate.GetFee(virtual_size);
            std::string err_string;
            AssertLockNotHeld(cs_main);
            NodeContext& node = EnsureAnyNodeContext(request.context);
            const CTransaction& ptx = *tx;  

            if(ptx.nVersion != TRANSACTION_PRECONF_VERSION) {
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "asset data missing to submit in mempool");
            }

            const TransactionError err = BroadcastTransaction(node, tx, err_string, max_raw_tx_fee, /*relay=*/true, /*wait_callback=*/true, true);
            if (TransactionError::OK != err) {
                throw JSONRPCTransactionError(err, err_string);
            }
                
            return tx->GetHash().GetHex();
            
        },
    };
}

static RPCHelpMan sendpreconfsignatures()
{
    return RPCHelpMan{"sendpreconfsignatures",
        "\nSubmit a pre conf transaction signatures from federation \n",
        {
            {"signatures",RPCArg::Type::ARR, RPCArg::Optional::NO, "pre signed preconf signature details from anduro",
              {
                {"", RPCArg::Type::OBJ, RPCArg::Optional::NO, "",
                    {
                        {"txid", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "Transaction id which is in preconf mempool"},
                        {"utc_time", RPCArg::Type::NUM, RPCArg::Optional::NO, "Transaction time anduro see it from their node"},
                        {"block_height", RPCArg::Type::NUM, RPCArg::Optional::NO, "the block height where the signatures get invalidated. the signature will be deleted after the block height"},
                        {"position", RPCArg::Type::NUM, RPCArg::Optional::NO, "The anduro position to sign the transaction"},
                        {"witness", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "The anduro signature"},
                    },
                }
              }
            },
        },
        RPCResult{
            RPCResult::Type::BOOL, "", "the result for preconf signature submission"
        },
        RPCExamples{
            "\nSend the signature for preconf transaction\n"
            + HelpExampleCli("sendpreconfsignatures", "\"[{\\\"txid\\\" : \\\"mytxid\\\",\\\"utc_time\\\":0,\\\"block_height\\\":0,\\\"position\\\":0,\\\"witness\\\" : \\\"witness\\\"}]\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            ChainstateManager& chainman = EnsureAnyChainman(request.context);
            const UniValue& req_params = request.params[0].get_array();
            if(req_params.size() > 0) {
                std::vector<CoordinatePreConfSig> preconf;
                for (unsigned int idx = 0; idx < req_params.size(); idx++) {
                    const UniValue& fedParams = req_params[idx].get_obj();
                    RPCTypeCheckObj(fedParams,
                    {
                        {"txid", UniValueType(UniValue::VSTR)},
                        {"utc_time", UniValueType(UniValue::VNUM)},
                        {"block_height", UniValueType(UniValue::VNUM)},
                        {"position", UniValueType(UniValue::VNUM)},
                        {"witness", UniValueType(UniValue::VSTR)},
                    });
                    CoordinatePreConfSig preconfObj;
                    preconfObj.txid =  ParseHashO(fedParams, "txid");
                    preconfObj.utcTime =  find_value(fedParams, "utc_time").getInt<int64_t>();
                    preconfObj.blockHeight =  find_value(fedParams, "block_height").getInt<int32_t>();
                    preconfObj.anduroPos =  find_value(fedParams, "position").getInt<int32_t>();
                    preconfObj.witness =  find_value(fedParams, "witness").get_str();
                    preconf.push_back(preconfObj);
                }
                return includePreConfSigWitness(preconf, chainman);
            }
            return false;
        },
    };
}

static RPCHelpMan getpreconffee()
{
    return RPCHelpMan{"getpreconffee",
        "\nGet current preconfirmation fee \n",
        {
        },
        RPCResult{
            RPCResult::Type::NUM, "", "current preconf minimum fee"
        },
        RPCExamples{
            "\nGet current transaciton fee\n"
            + HelpExampleCli("sendpreconfsignatures", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            ChainstateManager& chainman = EnsureAnyChainman(request.context);
            
            CChain& active_chain = chainman.ActiveChain();
            int blockindex = active_chain.Height();
            CBlock block;
            if (!ReadBlockFromDisk(block, CHECK_NONFATAL(active_chain[blockindex]), Params().GetConsensus())) {
                LogPrintf("Error reading block from disk at index %d\n", CHECK_NONFATAL(active_chain[blockindex])->GetBlockHash().ToString());
                return 0;
            }

            return block.preconfMinFee;
        },
    };
}

static RPCHelpMan getpreconfvotes()
{
    return RPCHelpMan{"getpreconfvotes",
        "\nGet current getpreconf votes list \n",
        {
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::ARR, "votes", "",
                {
                     {RPCResult::Type::OBJ, "", "",
                        {
                            {RPCResult::Type::STR, "txid", "preconf transaction txid"},
                            {RPCResult::Type::NUM, "votes", "total votes by anduro"},
                        }
                     }
                }},
            },
        },
        RPCExamples{
            "\nGet current transaciton fee\n"
            + HelpExampleCli("sendpreconfsignatures", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            UniValue result(UniValue::VOBJ);
            UniValue votes(UniValue::VARR);
            std::vector<CoordinatePreConfVotes> coordinatePreConfVotes = getPreConfVotes();
            for (const CoordinatePreConfVotes& coordinatePreConfVote : coordinatePreConfVotes) {
                UniValue voteItem(UniValue::VOBJ);
                voteItem.pushKV("txid", coordinatePreConfVote.txid.ToString());
                voteItem.pushKV("votes", coordinatePreConfVote.votes);
                votes.push_back(voteItem);
            }
            result.pushKV("votes", votes);
            return result;
        },
    };
}

static RPCHelpMan listAllSignedBlocks() {
        return RPCHelpMan{
        "listallsignedblocks",
        "get all signed blocks",
        {
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::ARR, "blocks", "",
                {
                     {RPCResult::Type::OBJ, "", "",
                        {
                            {RPCResult::Type::NUM, "height", "Signed block height"},
                            {RPCResult::Type::STR_HEX, "hash", "Signed block hash"},
                            {RPCResult::Type::NUM, "txcount", "Signed block transaction count"},
                        }
                     }
                }},
            },
        },
        RPCExamples{
           HelpExampleCli("listallsignedblocks", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            NodeContext& node = EnsureAnyNodeContext(request.context);
            const CTxMemPool& mempool = EnsureMemPool(node);
            ChainstateManager& chainman = EnsureChainman(node);

                UniValue result(UniValue::VOBJ);
                UniValue assets(UniValue::VARR);
                std::vector<SignedBlock> blockList = chainman.ActiveChainstate().psignedblocktree->GetSignedBlocks();
            ;
                int incr = 0;
                for (const SignedBlock& block_item : blockList) {
                    UniValue obj(UniValue::VOBJ);
                    obj.pushKV("height", (uint64_t)block_item.nHeight + incr);
                    obj.pushKV("assettype", block_item.GetHash().ToString());
                    obj.pushKV("txcount", block_item.vtx.size());
                    assets.push_back(obj);
                    incr = incr + 1;
                }
                result.pushKV("blocks", assets);
                return result;
        }
    };

}

void RegisterPreConfMempoolRPCCommands(CRPCTable& t)
{
    static const CRPCCommand commands[]{
        {"preconf", &sendpreconftransaction},
        {"preconf", &sendpreconfsignatures},
        {"preconf", &getpreconffee},
        {"preconf", &getpreconfvotes},
        {"preconf", &listAllSignedBlocks},
    };
    for (const auto& c : commands) {
        t.appendCommand(c.name, &c);
    }
}
