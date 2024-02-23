
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
                        {"block_height", RPCArg::Type::NUM, RPCArg::Optional::NO, "the block height where the signatures get invalidated. the signature will be deleted after the block height"},
                        {"mined_block_height", RPCArg::Type::NUM, RPCArg::Optional::NO, "the mined block height where the federation is refer to for sig validation"},
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
            + HelpExampleCli("sendpreconfsignatures", "\"[{\\\"txid\\\":\\\"mytxid\\\",\\\"block_height\\\":0,\\\"mined_block_height\\\":0,\\\"position\\\":0,\\\"witness\\\" : \\\"witness\\\"}]\"")
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
                        {"block_height", UniValueType(UniValue::VNUM)},
                        {"mined_block_height", UniValueType(UniValue::VNUM)},
                        {"position", UniValueType(UniValue::VNUM)},
                        {"witness", UniValueType(UniValue::VSTR)},
                    });
                    CoordinatePreConfSig preconfObj;
                    preconfObj.txid =  ParseHashO(fedParams, "txid");
                    preconfObj.blockHeight =  find_value(fedParams, "block_height").getInt<int32_t>();
                    preconfObj.minedBlockHeight =  find_value(fedParams, "mined_block_height").getInt<int32_t>();
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
            return getPreConfMinFee();
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

static RPCHelpMan getsignedblock() {
        return RPCHelpMan{
        "getsignedblock",
        "get signed block detail",
        {
            {"height", RPCArg::Type::STR, RPCArg::Optional::NO, "The height index"},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {
                    {RPCResult::Type::NUM, "fee", "Signed block fee"},
                    {RPCResult::Type::NUM, "blockindex", "block index where anduro witness refer back to the pubkeys"},
                    {RPCResult::Type::NUM, "height", "Signed block height"},
                    {RPCResult::Type::NUM, "time", "Signed block time"},
                    {RPCResult::Type::NUM, "time", "Signed block time"},
                    {RPCResult::Type::STR_HEX, "previousblock", "previous signed block hash"},
                    {RPCResult::Type::STR_HEX, "merkleroot", "signed block merkle root hash"},
                    {RPCResult::Type::STR_HEX, "hash", "Signed block hash"},
                    {RPCResult::Type::ARR, "tx", "The transaction ids",
                    {{RPCResult::Type::STR_HEX, "", "The transaction id"}}},
                }
            },
        },
        RPCExamples{
           HelpExampleCli("getsignedblock", "1000")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            ChainstateManager& chainman = EnsureAnyChainman(request.context);
            LOCK(cs_main);
            uint64_t nHeight = 0;
            ParseUInt64(request.params[0].get_str(),&nHeight);

            UniValue result(UniValue::VOBJ);
            SignedBlock block;
            chainman.ActiveChainstate().psignedblocktree->GetSignedBlock(nHeight,block);

            if (block.nHeight == 0)
               throw JSONRPCError(RPC_INVALID_PARAMETER, "Signed Block height out of range");
            
            UniValue txs(UniValue::VARR);
            for (const CTransactionRef& tx : block.vtx) {
                txs.push_back(tx->GetHash().ToString());
            }
            result.pushKV("fee", block.currentFee);
            result.pushKV("blockindex", (uint64_t)block.blockIndex);
            result.pushKV("height", (uint64_t)block.nHeight);
            result.pushKV("time", block.nTime);
            result.pushKV("previousblock", block.hashPrevSignedBlock.ToString());
            result.pushKV("merkleroot", block.hashMerkleRoot.ToString());
            result.pushKV("hash", block.GetHash().ToString());
            result.pushKV("tx", txs);
            return result;
        }
    };

}
static RPCHelpMan getsignedblocklist() {
        return RPCHelpMan{
        "getsignedblocklist" ,
        "get signed block detail",
        {
            {"startingHeight", RPCArg::Type::STR, RPCArg::Optional::NO, "The starting height index"},
            {"range", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "The range of heights to retrieve signed block details"},
        },

        RPCResult{
            RPCResult::Type::ARR, "", "",
            {
                {
                    {RPCResult::Type::OBJ, "", "Signed block details",
                    {
                        {RPCResult::Type::NUM, "fee", "Signed block fee"},
                        {RPCResult::Type::NUM, "blockindex", "block index where anduro witness refer back to the pubkeys"},
                        {RPCResult::Type::NUM, "height", "Signed block height"},
                        {RPCResult::Type::NUM, "time", "Signed block time"},
                        {RPCResult::Type::STR_HEX, "previousblock", "previous signed block hash"},
                        {RPCResult::Type::STR_HEX, "merkleroot", "signed block merkle root hash"},
                        {RPCResult::Type::STR_HEX, "hash", "Signed block hash"},
                        {RPCResult::Type::ARR, "tx", "The transaction ids",
                            {{RPCResult::Type::STR_HEX, "", "The transaction id"}}},
                    }},
                },
            },
        },
        RPCExamples{
            HelpExampleCli("getsignedblocklist", "146 20"),

        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            ChainstateManager& chainman = EnsureAnyChainman(request.context);
            uint64_t startingHeight;
            uint64_t range;
            uint64_t nHeight = 0;
            chainman.ActiveChainstate().psignedblocktree->GetLastSignedBlockID(nHeight);
            if(request.params[1].isNull()){
                range = 10;
            }
            if (!ParseUInt64(request.params[0].get_str(), &startingHeight) || (request.params.size() > 1) && !ParseUInt64(request.params[1].get_str(), &range) || 
              range == 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Height and range must be positive numbers");
              }
              if (range > 100) {
                 throw JSONRPCError(RPC_INVALID_PARAMETER, "Maximum range allowed is 100");
              }
             if (startingHeight + range > nHeight) {
                  throw JSONRPCError(RPC_INVALID_PARAMETER, "Starting height and range exceed the block count.");
               }

            LOCK(cs_main);
            UniValue result(UniValue::VARR);
            for (uint64_t i = startingHeight + 1; i <= startingHeight + range; ++i) {
                SignedBlock block;
                chainman.ActiveChainstate().psignedblocktree->GetSignedBlock(i, block);
                
                UniValue blockDetails(UniValue::VOBJ); 
                UniValue txs(UniValue::VARR);
                for (const CTransactionRef& tx : block.vtx) {
                    txs.push_back(tx->GetHash().ToString());
                }

                blockDetails.pushKV("fee", block.currentFee);
                blockDetails.pushKV("blockindex", (uint64_t)block.blockIndex);
                blockDetails.pushKV("height", (uint64_t)block.nHeight);
                blockDetails.pushKV("time", block.nTime);
                blockDetails.pushKV("previousblock", block.hashPrevSignedBlock.ToString());
                blockDetails.pushKV("merkleroot", block.hashMerkleRoot.ToString());
                blockDetails.pushKV("hash", block.GetHash().ToString());
                blockDetails.pushKV("tx", txs);

                result.push_back(blockDetails); 
            }

            return result;
        }
    };
}


static RPCHelpMan getsignedblockcount()
{
    return RPCHelpMan{"getsignedblockcount",
                "\nReturns the height of the signed block count\n"
                "The genesis block has height 1.\n",
                {},
                RPCResult{
                    RPCResult::Type::NUM, "", "The current signed block count"},
                RPCExamples{
                    HelpExampleCli("getsignedblockcount", "")
            + HelpExampleRpc("getsignedblockcount", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    ChainstateManager& chainman = EnsureAnyChainman(request.context);
    LOCK(cs_main);
    uint64_t nHeight = 0;
    chainman.ActiveChainstate().psignedblocktree->GetLastSignedBlockID(nHeight);
    return nHeight;
},
    };
}


void RegisterPreConfMempoolRPCCommands(CRPCTable& t)
{
    static const CRPCCommand commands[]{
        {"preconf", &sendpreconftransaction},
        {"preconf", &sendpreconfsignatures},
        {"preconf", &getpreconffee},
        {"preconf", &getpreconfvotes},
        {"preconf", &getsignedblock},
        {"preconf", &getsignedblockcount},
        {"preconf", &getsignedblocklist},
    };
    for (const auto& c : commands) {
        t.appendCommand(c.name, &c);
    }
}
