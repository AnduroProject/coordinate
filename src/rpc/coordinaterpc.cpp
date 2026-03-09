#include <rpc/coordinaterpc.h>
#include <common/args.h>
#include <rpc/util.h>
#include <rpc/server.h>
#include <rpc/server_util.h>
#include <key_io.h>
#include <rpc/auxpow_miner.h>
#include <core_io.h>
#include <coordinate/anduro_deposit.h>
#include <coordinate/coordinate_mempool_entry.h>
#include <coordinate/coordinate_pegin.h>
#include <coordinate/anduro_validator.h>
#include <rpc/request.h>

using node::NodeContext;

static RPCHelpMan createAuxBlock()
{
    return RPCHelpMan{
        "createauxblock",
        "The mining pool to request new coordinate block header hashes",
        {
            {"paytoaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "The coordinate address that the mining pool wants to send the Babylon mining rewards to. Will be written to coordinate coinbase transaction."},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR, "bits", "The target difficulty of coordinate. The larger the bits, the higher the difficulty. Target hash in int = 2^256 / int(bits)."},
                {RPCResult::Type::NUM, "chainid", "coordinate identity"},
                {RPCResult::Type::NUM, "height", "Height of transactions for the block"},
                {RPCResult::Type::NUM, "coinbasevalue", "Miner reward for the block"},
                {RPCResult::Type::STR_HEX, "hash", "The header hash of the created coordinate block"},
                {RPCResult::Type::STR_HEX, "previousblockhash", "The previous hash of the created coordinate block"},
          
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

static RPCHelpMan createAuxBlockHex()
{
    return RPCHelpMan{
        "createauxblockhex",
        "The mining pool to request new coordinate block hex",
        {
            {"paytoaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "The coordinate address that the mining pool wants to send the Babylon mining rewards to. Will be written to coordinate coinbase transaction."},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
               {RPCResult::Type::STR_HEX, "hex", "The coordinate block hex"},
                {RPCResult::Type::OBJ, "aux", "aux object",
                    {
                        {RPCResult::Type::NUM, "n", "block_height"},
                    }
                }
            },
        },
        RPCExamples{
            HelpExampleCli("createauxblockhex", "90869d013db27608c7428251c6755e5a1d9e9313") +
            HelpExampleRpc("createauxblockhex", "\"90869d013db27608c7428251c6755e5a1d9e9313\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            const CTxDestination coinbaseScript = DecodeDestination(request.params[0].get_str());
            if (!IsValidDestination(coinbaseScript)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,"Error: Invalid coinbase payout address");
            }
            const CScript scriptPubKey = GetScriptForDestination(coinbaseScript);
            return AuxpowMiner::get ().createAuxBlockHex(request, scriptPubKey);
        }
    };
}


static RPCHelpMan submitAuxBlock()
{
    return RPCHelpMan{
        "submitauxblock",
        "The mining pool to submit the mined auxPoW",
        {
            {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "The coordinate bloch header hash that the submitted auxPoW is associated with."},
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



static RPCHelpMan anduroDepositAddress()
{
    return RPCHelpMan{
        "andurodepositaddress",
        "Anduro current deposit address",
        {},
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR, "result", /*optional=*/true, "Returns anduro deposit address"},
            },
        },
        RPCExamples{
            HelpExampleCli("andurodepositaddress", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            return getDepositAddress();
        }
    };

}

static RPCHelpMan anduroWithdrawAddress()
{
    return RPCHelpMan{
        "anduroburnaddress",
        "Anduro current burn address",
        {},
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR, "result", /*optional=*/true, "Returns anduro burn address"},
            },
        },
        RPCExamples{
            HelpExampleCli("anduroburnaddress", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            return getBurnAddress();
        }
    };

}

static RPCHelpMan getPendingCommitments() {
        return RPCHelpMan{
        "getpendingcommitment",
        "get all pending precommitment signature for next block",
        {
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::ARR, "commitments", "",
                {
                     {RPCResult::Type::OBJ, "", "",
                        {
                            {RPCResult::Type::NUM, "n", "block_height"},
                        }
                     }
                }},
            },
        },
        RPCExamples{
           HelpExampleCli("getpendingcommitment", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
                UniValue result(UniValue::VOBJ);
                UniValue commitments(UniValue::VARR);
                std::vector<AnduroPreCommitment> cList = listPendingCommitment(-1);
                for (const AnduroPreCommitment& cItem : cList) {
                    UniValue tresult(UniValue::VOBJ);
                    tresult.pushKV("block_height", cItem.block_height);
                    commitments.push_back(tresult);
                }
                result.pushKV("commitments", commitments);
                return result;
        }
    };

}

static RPCHelpMan listAllAssets() {
        return RPCHelpMan{
        "listallassets",
        "get all coordinate assets",
        {
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::ARR, "assets", "",
                {
                     {RPCResult::Type::OBJ, "", "",
                        {
                            {RPCResult::Type::NUM, "id", "AssetID"},
                            {RPCResult::Type::NUM, "assettype", "Asset Type"},
                            {RPCResult::Type::NUM, "precision", "Precision Number"},
                            {RPCResult::Type::STR, "ticker", "Asset Ticker"},
                            {RPCResult::Type::NUM, "supply", "Asset supply"},
                            {RPCResult::Type::STR, "headline", "Asset title"},
                            {RPCResult::Type::STR, "payload", "Asset payload"},
                            {RPCResult::Type::STR, "txid", "Asset genesis"},
                            {RPCResult::Type::STR, "controller", "Asset controller"},
                            {RPCResult::Type::STR, "owner", "Asset owner"},
                        }
                     }
                }},
            },
        },
        RPCExamples{
           HelpExampleCli("listallassets", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            NodeContext& node = EnsureAnyNodeContext(request.context);
            ChainstateManager& chainman = EnsureChainman(node);
            UniValue result(UniValue::VOBJ);
            UniValue assets(UniValue::VARR);
            std::vector<CoordinateAsset> assetList = chainman.ActiveChainstate().passettree->GetAssets();
            ;

            for (const CoordinateAsset& asset_item : assetList) {
                uint64_t blockNumber;
                uint16_t assetIndex;
                ParseAssetId(asset_item.nID, blockNumber, assetIndex);

                UniValue obj(UniValue::VOBJ);
                obj.pushKV("id", assetIndex);
                obj.pushKV("blockheight", blockNumber);
                obj.pushKV("assettype", asset_item.assetType);
                obj.pushKV("precision", asset_item.precision);
                obj.pushKV("ticker", asset_item.strTicker);
                obj.pushKV("supply", asset_item.nSupply);
                obj.pushKV("headline", asset_item.strHeadline);
                obj.pushKV("payload", asset_item.payload.ToString());
                obj.pushKV("txid", asset_item.txid.ToString());
                obj.pushKV("controller", asset_item.strController);
                obj.pushKV("owner", asset_item.strOwner);
                assets.push_back(obj);
            }
            result.pushKV("assets", assets);
            return result;
        }};
}

static RPCHelpMan listMempoolAssets() {
        return RPCHelpMan{
        "listmempoolassets",
        "get all coordinate assets from mempool",
        {
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::ARR, "assets", "",
                {
                     {RPCResult::Type::OBJ, "", "",
                        {
                            {RPCResult::Type::NUM, "assetId", "AssetID"},
                            {RPCResult::Type::STR_HEX, "assetId", "Transaction ID"},
                            {RPCResult::Type::NUM, "vout", "Transaction Index"},
                            {RPCResult::Type::NUM, "nValue", "Transaction Amount"},
                        }
                     }
                }},
            },
        },
        RPCExamples{
           HelpExampleCli("listmempoolassets", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {

                UniValue result(UniValue::VOBJ);
                UniValue assets(UniValue::VARR);
                std::vector<CoordinateMempoolEntry> assetList = getMempoolAssets();
            ;

            for (const CoordinateMempoolEntry& assetItem : assetList) {
                UniValue obj(UniValue::VOBJ);
                uint64_t blockNumber;
                uint16_t assetIndex;
                ParseAssetId(assetItem.assetID, blockNumber, assetIndex);
                obj.pushKV("id", assetIndex);
                obj.pushKV("blockheight", blockNumber);
                obj.pushKV("txid", assetItem.txid.ToString());
                obj.pushKV("vout", (uint32_t)assetItem.vout);
                obj.pushKV("nValue", (int64_t)assetItem.nValue);
                assets.push_back(obj);
            }
            result.pushKV("assets", assets);
            return result;
        }};
}

static RPCHelpMan createPegin()
{
    return RPCHelpMan{"createpegin",
                "\nCreates a raw transaction to claim coins from the main chain by creating a pegin transaction with the necessary metadata after the corresponding Bitcoin transaction.\n"
                "Note that this call will not sign the transaction.\n"
                "If a transaction is not relayed it may require manual addition to a functionary mempool in order for it to be mined.\n",
                {
                    {"bitcoin_tx", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "The raw bitcoin transaction (in hex) depositing bitcoin to the mainchain_address generated by getpeginaddress"},
                    {"txoutproof", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "A rawtxoutproof (in hex) generated by the mainchain daemon's `gettxoutproof` containing a proof of only bitcoin_tx"},
                    {"deposit_address", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "deposit adddress generated by federation"},
                    {"miner_address", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "miner adddress to receive fee"},
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::STR, "hex", "raw transaction data"},
                        {RPCResult::Type::BOOL, "mature", "Whether the peg-in is mature (only included when validating peg-ins)"},
                    },
                },
                RPCExamples{
                    HelpExampleCli("createrawpegin", "\"0200000002b80a99d63ca943d72141750d983a3eeda3a5c5a92aa962884ffb141eb49ffb4f000000006a473044022031ffe1d76decdfbbdb7e2ee6010e865a5134137c261e1921da0348b95a207f9e02203596b065c197e31bcc2f80575154774ac4e80acd7d812c91d93c4ca6a3636f27012102d2130dfbbae9bd27eee126182a39878ac4e117d0850f04db0326981f43447f9efeffffffb80a99d63ca943d72141750d983a3eeda3a5c5a92aa962884ffb141eb49ffb4f010000006b483045022100cf041ce0eb249ae5a6bc33c71c156549c7e5ad877ae39e2e3b9c8f1d81ed35060220472d4e4bcc3b7c8d1b34e467f46d80480959183d743dad73b1ed0e93ec9fd14f012103e73e8b55478ab9c5de22e2a9e73c3e6aca2c2e93cd2bad5dc4436a9a455a5c44feffffff0200e1f5050000000017a914da1745e9b549bd0bfa1a569971c77eba30cd5a4b87e86cbe00000000001976a914a25fe72e7139fd3f61936b228d657b2548b3936a88acc0020000\", \"00000020976e918ed537b0f99028648f2a25c0bd4513644fb84d9cbe1108b4df6b8edf6ba715c424110f0934265bf8c5763d9cc9f1675a0f728b35b9bc5875f6806be3d19cd5b159ffff7f2000000000020000000224eab3da09d99407cb79f0089e3257414c4121cb85a320e1fd0f88678b6b798e0713a8d66544b6f631f9b6d281c71633fb91a67619b189a06bab09794d5554a60105\" \"0014058c769ffc7d12c35cddec87384506f536383f9c\"")
            + HelpExampleRpc("createrawpegin", "\"0200000002b80a99d63ca943d72141750d983a3eeda3a5c5a92aa962884ffb141eb49ffb4f000000006a473044022031ffe1d76decdfbbdb7e2ee6010e865a5134137c261e1921da0348b95a207f9e02203596b065c197e31bcc2f80575154774ac4e80acd7d812c91d93c4ca6a3636f27012102d2130dfbbae9bd27eee126182a39878ac4e117d0850f04db0326981f43447f9efeffffffb80a99d63ca943d72141750d983a3eeda3a5c5a92aa962884ffb141eb49ffb4f010000006b483045022100cf041ce0eb249ae5a6bc33c71c156549c7e5ad877ae39e2e3b9c8f1d81ed35060220472d4e4bcc3b7c8d1b34e467f46d80480959183d743dad73b1ed0e93ec9fd14f012103e73e8b55478ab9c5de22e2a9e73c3e6aca2c2e93cd2bad5dc4436a9a455a5c44feffffff0200e1f5050000000017a914da1745e9b549bd0bfa1a569971c77eba30cd5a4b87e86cbe00000000001976a914a25fe72e7139fd3f61936b228d657b2548b3936a88acc0020000\", \"00000020976e918ed537b0f99028648f2a25c0bd4513644fb84d9cbe1108b4df6b8edf6ba715c424110f0934265bf8c5763d9cc9f1675a0f728b35b9bc5875f6806be3d19cd5b159ffff7f2000000000020000000224eab3da09d99407cb79f0089e3257414c4121cb85a320e1fd0f88678b6b798e0713a8d66544b6f631f9b6d281c71633fb91a67619b189a06bab09794d5554a60105\", \"0014058c769ffc7d12c35cddec87384506f536383f9c\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    if (!IsHex(request.params[0].get_str()) || !IsHex(request.params[1].get_str())) {
        throw JSONRPCError(RPC_TYPE_ERROR, "the first two arguments must be hex strings");
    }
    LOCK(cs_main);
    NodeContext& node = EnsureAnyNodeContext(request.context);
    ChainstateManager& chainman = EnsureChainman(node);

    if(!hasAddressInRegistry(chainman.ActiveChainstate(),request.params[2].get_str())) {
        throw JSONRPCError(RPC_TYPE_ERROR, "deposit address not exist");
    }

    CTxOut txOut = getPeginAmount(ParseHex(request.params[0].get_str()), ParseHex(request.params[1].get_str()), request.params[2].get_str());

    if(txOut.IsNull()) {
        throw JSONRPCError(RPC_TYPE_ERROR, "error geting pegin output");
    }

    CMutableTransaction mtx;
    mtx.version = TRANSACTION_PEGIN_VERSION;
    mtx.vin.push_back(buildPeginTxInput(ParseHex(request.params[0].get_str()), ParseHex(request.params[1].get_str()),  request.params[2].get_str(), txOut));
    mtx.vout.push_back(txOut);
    const CTxDestination coinbaseScript = DecodeDestination(request.params[3].get_str());
    mtx.vout.push_back(CTxOut(0, GetScriptForDestination(coinbaseScript)));

    CTransaction ctx = CTransaction(mtx);
    size_t size(GetVirtualTransactionSize(ctx));
    LogPrintf("tx size %i \n",size);
    LogPrintf("original value %i \n",mtx.vout[0].nValue);
    mtx.vout[0].nValue = mtx.vout[0].nValue - (size * PEGIN_FEE);
    mtx.vout[1].nValue = size * PEGIN_FEE;
    LogPrintf("final value %i \n",mtx.vout[0].nValue);


    std::string strHex = EncodeHexTx(CTransaction(mtx));
    std::string err;
    bool valid = IsValidPeginWitness(mtx.vin[0].scriptWitness, mtx.vin[0].prevout, err);

    LogPrintf(" message %s \n",err);

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("hex", strHex);
    obj.pushKV("mature", valid);
    return obj;
},
    };
}

void RegisterCoordinateRPCCommands(CRPCTable& t)
{
    static const CRPCCommand commands[]{
        {"coordinate", &createAuxBlock},
        {"coordinate", &createAuxBlockHex},
        {"coordinate", &submitAuxBlock},
        {"coordinate", &getPendingCommitments},
        {"coordinate", &anduroDepositAddress},
        {"coordinate", &anduroWithdrawAddress},
        {"coordinate", &listAllAssets},
        {"coordinate", &listMempoolAssets},
        {"coordinate", createPegin}
    };
    for (const auto& c : commands) {
        t.appendCommand(c.name, &c);
    }
}


