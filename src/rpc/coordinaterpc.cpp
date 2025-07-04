#include <rpc/coordinaterpc.h>
#include <rpc/util.h>
#include <rpc/server.h>
#include <rpc/server_util.h>
#include <key_io.h>
#include <rpc/auxpow_miner.h>
#include <core_io.h>
#include <anduro_deposit.h>
#include <coordinate/coordinate_mempool_entry.h>
#include <anduro_validator.h>
#include <consensus/merkle.h>
#include <merkleblock.h>

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
            // raw block proof
            std::vector<uint256> leaves;
            leaves.resize(1);
            leaves[0].SetNull();
            leaves.push_back(uint256S("992f701a2959345ab1579445a14c12d06ade6d65f0b89bdc3dbb32ec35e4ae30"));
            leaves.push_back(uint256S("013e8776b43a71e9d9dc80662c4b4bf6bb87093794f41c24ee8786aed8626eb5"));
            leaves.push_back(uint256S("4679dfbd42be169a953d6586f995cdd6c50b89c2a9a87e55d05faaa484309021"));
            leaves.push_back(uint256S("7df3aa51463c3ca96639efb43fb445dca1662f427e9344f4f4a66e4a80de4db1"));
            leaves.push_back(uint256S("88611613c3c74af87397c7af13b08bfce4bd4f4081c1d8ae2613ec715b830c95"));

            uint256 root = ComputeMerkleRoot(leaves);
       
            // invalid block proof after 3 blocks
            std::vector<uint256> rleaves;
            rleaves.resize(6);
            rleaves[0].SetNull();
            rleaves[1].SetNull();
            rleaves[2].SetNull();
            rleaves[3].SetNull();
            rleaves[4] = uint256S("7df3aa51463c3ca96639efb43fb445dca1662f427e9344f4f4a66e4a80de4db1");
            rleaves[5].SetNull();
            uint256 rroot = ComputeMerkleRoot(rleaves);


            LogPrintf("root is %s \n", root.ToString());
            LogPrintf("reconcile root is %s \n", rroot.ToString());


            // check proof for Raw block
            std::vector<uint256> input;
            // input.resize(1);
            // input[0].SetNull();
            input.push_back(uint256S("4679dfbd42be169a953d6586f995cdd6c50b89c2a9a87e55d05faaa484309021"));

            // prepare proof
            std::vector<bool> vMatch;
            std::vector<uint256> vHashes;
            vMatch.reserve(leaves.size());
            vHashes.reserve(leaves.size());

            for (unsigned int i = 0; i < leaves.size(); i++)
            {
                const uint256& hash = leaves[i];
                if (std::find(input.begin(), input.end(), hash) != input.end()) {
                    vMatch.push_back(true);
                } else {
                    vMatch.push_back(false);
                }
                vHashes.push_back(hash);
                LogPrintf("element is %s \n", hash.ToString());
            }

            std::vector<uint256> rvHashes;
            rvHashes.reserve(rleaves.size());
            for (unsigned int i = 0; i < rleaves.size(); i++) {
                const uint256& hash = rleaves[i];
                rvHashes.push_back(hash);
                LogPrintf("reconcile element is %s \n", hash.ToString());
            }

            DataStream ssMB{};
            CPartialMerkleTree txn(vHashes, vMatch);
            ssMB << txn;

            DataStream ssMB1{};
            CPartialMerkleTree txn1(rvHashes, vMatch);
            ssMB1 << txn1;

            LogPrintf("proof is %s \n", HexStr(ssMB));
            LogPrintf("reconcile proof is %s \n", HexStr(ssMB1));

            DataStream ssEB{ParseHexV(HexStr(ssMB), "proof")};
            CPartialMerkleTree pMerkleBlock;
            ssEB >> pMerkleBlock;

            DataStream ssEB1{ParseHexV(HexStr(ssMB1), "proof")};
            CPartialMerkleTree pMerkleBlock1;
            ssEB1 >> pMerkleBlock1;

            std::vector<uint256> vMatchE;
            std::vector<unsigned int> vIndexE;
            uint256 finalVerify(pMerkleBlock.ExtractMatches(vMatchE, vIndexE));
            if(finalVerify != root) {
                LogPrintf("invalid proof %s \n", finalVerify.ToString());
                return getDepositAddress();
            }

            for (unsigned int i = 0; i < vMatchE.size(); i++) {
                const uint256& hash = vMatchE[i];
                LogPrintf("result element is %s \n", hash.ToString());
            }

            std::vector<uint256> vMatchE1;
            std::vector<unsigned int> vIndexE1;
            uint256 finalVerify1(pMerkleBlock1.ExtractMatches(vMatchE1, vIndexE1));
            if(finalVerify1 != rroot) {
                LogPrintf("invalid reconcile proof %s \n", finalVerify1.ToString());
               // return getDepositAddress();
            }

            for (unsigned int i = 0; i < vMatchE1.size(); i++) {
                const uint256& hash = vMatchE1[i];
                LogPrintf("result reconcile element is %s \n", hash.ToString());
            }

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
                    UniValue obj(UniValue::VOBJ);
                    obj.pushKV("id", (uint64_t)asset_item.nID);
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
        }
    };

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
                    obj.pushKV("assetId", (uint32_t)assetItem.assetID);
                    obj.pushKV("txid", assetItem.txid.ToString());
                    obj.pushKV("vout", (uint32_t)assetItem.vout);
                     obj.pushKV("nValue", (int64_t)assetItem.nValue);
                    assets.push_back(obj);
                }
                result.pushKV("assets", assets);
                return result;
        }
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
        {"coordinate", &listMempoolAssets}
    };
    for (const auto& c : commands) {
        t.appendCommand(c.name, &c);
    }
}


