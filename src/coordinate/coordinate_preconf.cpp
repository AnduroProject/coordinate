#include <coordinate/coordinate_preconf.h>
#include <core_io.h>
#include <logging.h>
#include <univalue.h>
#include <node/blockstorage.h>
#include <anduro_validator.h>
#include <validation.h>
#include <consensus/merkle.h>
#include <coordinate/invalid_tx.h>
#include <undo.h>

using node::BlockManager;
using node::ReadBlockFromDisk;
using node::UndoReadFromDisk;

std::vector<CoordinatePreConfSig> coordinatePreConfSig;

// temporary minfee
CAmount preconfMinFee = 5;

CoordinatePreConfBlock getNextPreConfSigList(ChainstateManager& chainman) {
    uint64_t signedBlockHeight = 0;
    chainman.ActiveChainstate().psignedblocktree->GetLastSignedBlockID(signedBlockHeight);
   LOCK(cs_main);
    CChain& active_chain = chainman.ActiveChain();
    int blockindex = active_chain.Height();
    CBlock block;
    if (!ReadBlockFromDisk(block, CHECK_NONFATAL(active_chain[blockindex]), Params().GetConsensus())) {
        LogPrintf("Error reading block from disk at index %d\n", CHECK_NONFATAL(active_chain[blockindex])->GetBlockHash().ToString());
        CoordinatePreConfBlock result;
        return result;
    }
    
    int finalizedStatus = 1;
    auto it = std::find_if(coordinatePreConfSig.begin(), coordinatePreConfSig.end(), 
            [finalizedStatus] (const CoordinatePreConfSig& d) { 
                return (uint64_t)d.finalized == finalizedStatus;
            });

    if (it == coordinatePreConfSig.end()) {
        CoordinatePreConfBlock result;
        return result;
    }
    
    CTxMemPool& preconf_pool{*chainman.ActiveChainstate().GetPreConfMempool()};
    CAmount finalFee = nextBlockFee(preconf_pool, signedBlockHeight+1);
    return prepareRefunds(preconf_pool,finalFee, signedBlockHeight+1);
}

CAmount nextBlockFee(CTxMemPool& preconf_pool, uint64_t signedBlockHeight) {
    CAmount finalFee = 0;
    for (const CoordinatePreConfSig& coordinatePreConfSigtem : coordinatePreConfSig) {
        if((uint64_t)coordinatePreConfSigtem.blockHeight != signedBlockHeight) {
           continue;
        }
        if(coordinatePreConfSigtem.txid.IsNull()) {
          continue;
        }
        CTransactionRef tx = preconf_pool.get(coordinatePreConfSigtem.txid);
        if(finalFee == 0 || finalFee > tx->vout[0].nValue) {
            finalFee = tx->vout[0].nValue;
        }
    } 
    return finalFee;
}

CoordinatePreConfBlock prepareRefunds(CTxMemPool& preconf_pool, CAmount finalFee, uint64_t signedBlockHeight) {
    std::vector<uint256> txids;
    CoordinatePreConfBlock result;
    LogPrintf("prepareRefunds check 1 %i \n", finalFee);
    for (const CoordinatePreConfSig& coordinatePreConfSigtem : coordinatePreConfSig) {    
        LogPrintf("prepareRefunds check 2 %i\n", signedBlockHeight); 
        if((uint64_t)coordinatePreConfSigtem.blockHeight != signedBlockHeight) {
           continue;
        }
        LogPrintf("prepareRefunds check 3 %s\n", coordinatePreConfSigtem.witness); 
        result.witness = coordinatePreConfSigtem.witness;
        if(coordinatePreConfSigtem.txid.IsNull()) {
          continue;
        }
        txids.push_back(coordinatePreConfSigtem.txid);

    }
    LogPrintf("prepareRefunds check 4 \n"); 
    result.txids = txids;
    result.fee = finalFee;
    return result;
}

bool includePreConfSigWitness(std::vector<CoordinatePreConfSig> preconf, ChainstateManager& chainman) {
    LOCK(cs_main);
    CChain& active_chain = chainman.ActiveChain();
    CTxMemPool& preconf_pool{*chainman.ActiveChainstate().GetPreConfMempool()};
    LOCK(preconf_pool.cs);

    uint64_t signedBlockHeight = 0;
    chainman.ActiveChainstate().psignedblocktree->GetLastSignedBlockID(signedBlockHeight);

    // check preconf data is empty
    if(preconf.size()==0) {
        LogPrintf("preconf transaction sig empty \n");
        return false;
    }

    // check preconf sign block height is valid   
    if((uint64_t)preconf[0].blockHeight <= signedBlockHeight) {
        LogPrintf("preconf transaction sig has old block height \n");
        return false;
    }

    int blockindex = preconf[0].minedBlockHeight;
    int finalizedStatus = preconf[0].finalized;

    // check txid exist in preconf mempool
    UniValue messages(UniValue::VARR);
    if(!preconf[0].txid.IsNull()) {
        for (const CoordinatePreConfSig& coordinatePreConfSigItem : preconf) {
            if(!preconf_pool.exists(GenTxid::Txid(coordinatePreConfSigItem.txid))) {
                    LogPrintf("preconf txid not avilable in mempool \n");
                    return false;
            }
        }
        //check signature is valid and include to queue
        for (const CoordinatePreConfSig& coordinatePreConfSigItem : preconf) {
            UniValue message(UniValue::VOBJ);
            message.pushKV("txid", coordinatePreConfSigItem.txid.ToString());
            message.pushKV("signed_block_height", coordinatePreConfSigItem.blockHeight);
            message.pushKV("mined_block_height", coordinatePreConfSigItem.minedBlockHeight);
            messages.push_back(message);
        }
    } else {
        //check signature is valid and include to queue if empty signature from federation
        UniValue message(UniValue::VOBJ);
        message.pushKV("txid", "");
        message.pushKV("signed_block_height", preconf[0].blockHeight);
        message.pushKV("mined_block_height", preconf[0].minedBlockHeight);
        messages.push_back(message);
    }


    // get block to find the eligible anduro keys to be signed on presigned block
    CBlock block;
    if (!ReadBlockFromDisk(block, CHECK_NONFATAL(active_chain[blockindex]), Params().GetConsensus())) {
        LogPrintf("Error reading block from disk at index %d\n", CHECK_NONFATAL(active_chain[blockindex])->GetBlockHash().ToString());
    }

    if(finalizedStatus == 1) {
        if(!validateAnduroSignature(preconf[0].witness,messages.write(),block.currentKeys)) {
            return false;
        }
        removePreConfWitness();
    } else {
        if(!validatePreconfSignature(preconf[0].witness,messages.write(),block.currentKeys)) {
            return false;
        }
    }



    for (const CoordinatePreConfSig& preconfItem : preconf) {
        coordinatePreConfSig.push_back(preconfItem);
    }

    return true;
}

void removePreConfWitness() {
    coordinatePreConfSig.clear();
}

/**
 * This is the function which used to get unbroadcasted preconfirmation signatures
 */
std::vector<CoordinatePreConfSig> getUnBroadcastedPreConfSig() {
    std::vector<CoordinatePreConfSig> sigData;
    for (CoordinatePreConfSig coordinatePreConfSigItem : coordinatePreConfSig) {
        if(!coordinatePreConfSigItem.isBroadcasted) {
            sigData.push_back(coordinatePreConfSigItem);
        } 
    }

    return sigData;
}

/**
 * This is the function which used to get all preconfirmation signatures
 */
std::vector<CoordinatePreConfSig> getPreConfSig() {
   return coordinatePreConfSig;
}

/**
 * This is the function which used change status for broadcasted preconf
 */
void updateBroadcastedPreConf(CoordinatePreConfSig& preconfItem, int64_t peerId) {
    for (CoordinatePreConfSig& coordinatePreConfSigItem : coordinatePreConfSig) {
        if(coordinatePreConfSigItem.witness.compare(preconfItem.witness)==0) {
            if (std::find(preconfItem.peerList.begin(), preconfItem.peerList.end(), peerId) != preconfItem.peerList.end()) {
                coordinatePreConfSigItem.isBroadcasted = true;
            } else {
                coordinatePreConfSigItem.peerList.push_back(peerId);
            }
            break;
        }
    }
}

/**
 * This is the function which used to get preconf vote information
 */
CAmount getPreConfMinFee() {
   return preconfMinFee;
}

/**
 * This function get new block template for signed block
 */
std::unique_ptr<SignedBlock> CreateNewSignedBlock(ChainstateManager& chainman, uint32_t nTime) {
    LOCK(cs_main);
   
    std::unique_ptr<SignedBlock> pblocktemplate;
    pblocktemplate.reset(new SignedBlock());
    SignedBlock *block = pblocktemplate.get();

    CCoinsViewCache view(&chainman.ActiveChainstate().CoinsTip());
    uint64_t nIDLast = 0;
    CoordinatePreConfBlock preconfList = getNextPreConfSigList(chainman);
    if(preconfList.witness.compare("")==0) {
        removePreConfWitness();
        LogPrintf("Signed block witness not exist \n");
        return nullptr;
    }

    SignedBlock prevBlock;
    chainman.ActiveChainstate().psignedblocktree->GetLastSignedBlockID(nIDLast);
    if(nIDLast > 0) {
        chainman.ActiveChainstate().psignedblocktree->GetSignedBlock(nIDLast,prevBlock);
        block->hashPrevSignedBlock = prevBlock.GetHash();
    }
    CTxMemPool& preconf_pool{*chainman.ActiveChainstate().GetPreConfMempool()};

    uint64_t nHeight = nIDLast + 1;
    block->currentFee = preconfList.fee;
    block->nHeight = nHeight;
    block->nTime = nTime;
    block->blockIndex = chainman.ActiveHeight();
    block->vtx.resize(preconfList.txids.size() + 1);

    unsigned int i = 1;
    std::vector<CTxOut> coinBaseOuts;

    std::vector<unsigned char> witnessData = ParseHex(preconfList.witness);
    CTxOut witnessOut(0, CScript() << OP_RETURN << witnessData);
    coinBaseOuts.push_back(witnessOut);

    for (const uint256& hash : preconfList.txids) {
        TxMempoolInfo info = preconf_pool.info(GenTxid::Txid(hash));
        if(!info.tx) {
           LogPrintf("Signed block invalid tx \n");
           return nullptr;
        }
        CAmount totalFee = preconfList.fee * info.vsize;
        CTxOut refund(info.fee - totalFee, info.tx->vout[0].scriptPubKey);
        coinBaseOuts.push_back(refund);
        block->vtx[i] = info.tx;
        i = i + 1;
    }

    std::vector<unsigned char> witnessMerkleData = ParseHex(SignedBlockWitnessMerkleRoot(*block).ToString());
    CTxOut witnessMerkleOut(0, CScript() << OP_RETURN << witnessMerkleData);
    coinBaseOuts.push_back(witnessMerkleOut);

    CMutableTransaction signedCoinbaseTx;
    signedCoinbaseTx.nVersion = TRANSACTION_PRECONF_VERSION;
    signedCoinbaseTx.vin.resize(1);
    signedCoinbaseTx.vin[0].prevout.SetNull();
    signedCoinbaseTx.vout.resize(coinBaseOuts.size());
    for (size_t i = 0; i < coinBaseOuts.size(); i++)
    {
        signedCoinbaseTx.vout[i] = coinBaseOuts[i];
    }
    block->vtx[0] = MakeTransactionRef(std::move(signedCoinbaseTx));
    block->hashMerkleRoot = SignedBlockMerkleRoot(*block);
    return pblocktemplate;
}

bool checkSignedBlock(const SignedBlock& block, ChainstateManager& chainman) {
    LOCK(cs_main);
    CChain& active_chain = chainman.ActiveChain();
    int blockindex = block.blockIndex;

    // check txid exist in preconf mempool
    UniValue messages(UniValue::VARR);
    if(block.vtx.size() > 1) {
        //check signature is valid and include to queue
        for (unsigned int i = 0; i < block.vtx.size(); i++) {
            if(i == 0) {
                continue;
            }
            UniValue message(UniValue::VOBJ);
            message.pushKV("txid", block.vtx[i]->GetHash().ToString());
            message.pushKV("signed_block_height", block.nHeight);
            message.pushKV("mined_block_height", block.blockIndex);
            messages.push_back(message);
        }
    } else {
        //check signature is valid and include to queue if empty signature from federation
        UniValue message(UniValue::VOBJ);
        message.pushKV("txid", "");
        message.pushKV("signed_block_height", block.nHeight);
        message.pushKV("mined_block_height", block.blockIndex);
        messages.push_back(message);
    }


    // get block to find the eligible anduro keys to be signed on presigned block
    CBlock minedblock;
    if (!ReadBlockFromDisk(minedblock, CHECK_NONFATAL(active_chain[blockindex]), Params().GetConsensus())) {
        removePreConfWitness();
        LogPrintf("Error reading block from disk at index %d\n", CHECK_NONFATAL(active_chain[blockindex])->GetBlockHash().ToString());
        return false;
    }

    CTxOut witnessOut = block.vtx[0]->vout[0];
    const std::string witnessStr = ScriptToAsmStr(witnessOut.scriptPubKey).replace(0,10,"");

    LogPrintf("preconf current keys %s \n", minedblock.currentKeys);
    LogPrintf("preconf message json %s \n", messages.write());
    LogPrintf("preconf witness %s \n", witnessStr);
    if(!validateAnduroSignature(witnessStr,messages.write(),minedblock.currentKeys)) {
       removePreConfWitness();
       return false;
    }

    return true;
}

/**
 * This function get next pre confs
 */
std::vector<SignedBlock> getNextPreConfs(ChainstateManager& chainman) {
    std::vector<SignedBlock> preconfs;
    LOCK(cs_main);
    CChain& active_chain = chainman.ActiveChain();

    int lastHeight = active_chain.Height();
    uint64_t startSignedBlockHeight = 0;
    uint64_t lastSignedBlockHeight = 0;
    chainman.ActiveChainstate().psignedblocktree->GetLastSignedBlockID(lastSignedBlockHeight);   

    if(lastSignedBlockHeight==0) {
        return preconfs;
    }

    if(lastHeight > 0) {
        bool findPreconf = false;
        while (!findPreconf) {
            CBlock prevblock;
            if (!ReadBlockFromDisk(prevblock, CHECK_NONFATAL(active_chain[lastHeight-1]), Params().GetConsensus())) {
                findPreconf = true;
                break;
            } 

            if(prevblock.preconfBlock.size() > 0) {
                uint256 lastPreconfBlock = prevblock.preconfBlock[prevblock.preconfBlock.size()-1];
                chainman.ActiveChainstate().psignedblocktree->getSignedBlockHeightByHash(lastPreconfBlock, lastSignedBlockHeight); 
                break;  
            }

            lastHeight = lastHeight-1;
            if(lastHeight == 0) {
                findPreconf = true;
            }
        }
    } else {
      startSignedBlockHeight = 1;
    }

   for (uint64_t i = startSignedBlockHeight; i <= lastSignedBlockHeight; i++) {
        SignedBlock prevBlock;
        chainman.ActiveChainstate().psignedblocktree->GetLastSignedBlockID(i);
        preconfs.push_back(prevBlock);
   }

   return preconfs;
}


/**
 * This function get next pre confs
 */
std::vector<uint256> getInvalidTx(ChainstateManager& chainman) {
    std::vector<uint256> invalidTxs;
    LOCK(cs_main);
    CChain& active_chain = chainman.ActiveChain();
    int lastHeight = active_chain.Height();
    if(lastHeight < 3 ) {
        return invalidTxs;
    }
    int currentHeight = lastHeight - 3;
    CBlock prevblock;
    if (!ReadBlockFromDisk(prevblock, CHECK_NONFATAL(active_chain[currentHeight]), Params().GetConsensus())) {
        return invalidTxs;
    } 
    InvalidTx invalidTxObj;
    chainman.ActiveChainstate().psignedblocktree->GetInvalidTx(currentHeight,invalidTxObj);
    return invalidTxObj.invalidTxs;
}


/**
 * This function is to get reconsiled block hash
 */
uint256 getReconsiledBlock(ChainstateManager& chainman) {
    LOCK(cs_main);
    CChain& active_chain = chainman.ActiveChain();
    int lastHeight = active_chain.Height();

    if(lastHeight < 3 ) {
        return uint256();
    }

    int currentHeight = lastHeight - 3;
    CBlock prevblock;
    if (!ReadBlockFromDisk(prevblock, CHECK_NONFATAL(active_chain[currentHeight]), Params().GetConsensus())) {
        return uint256();
    } 

    return prevblock.GetHash();
}


CAmount getPreconfFeeForBlock(ChainstateManager& chainman, int blockHeight) {
    LOCK(cs_main);
    CChain& active_chain = chainman.ActiveChain();
    if(blockHeight<3) {
        return 0;
    }
    int currentHeight = blockHeight - 3;

    CBlock prevblock;
    if (!ReadBlockFromDisk(prevblock, CHECK_NONFATAL(active_chain[currentHeight]), Params().GetConsensus())) {
        return 0;
    } 

    CAmount fee = 0;

    for (size_t i = 0; i < prevblock.preconfBlock.size(); i++)
    {
        uint64_t signedBlockIndex = 0;
        chainman.ActiveChainstate().psignedblocktree->getSignedBlockHeightByHash(prevblock.preconfBlock[i],signedBlockIndex);
        SignedBlock block;
        chainman.ActiveChainstate().psignedblocktree->GetSignedBlock(signedBlockIndex,block);
        for (size_t i = 0; i < block.vtx.size(); i++) {
            if(i==0) {
                continue;
            }
            const CTransactionRef& tx = block.vtx[i];
            fee = fee + (block.currentFee * ::GetSerializeSize(tx, SERIALIZE_TRANSACTION_NO_WITNESS));
        }
    }
    
    return fee;
}

CAmount getFeeForBlock(ChainstateManager& chainman, int blockHeight) {
    LOCK(cs_main);
    CChain& active_chain = chainman.ActiveChain();
    if(blockHeight<3) {
        return 0;
    }

    int currentHeight = blockHeight - 3;

    CBlock prevblock;
    if (!ReadBlockFromDisk(prevblock, CHECK_NONFATAL(active_chain[currentHeight]), Params().GetConsensus())) {
        return 0;
    } 


    CBlockUndo blockUndo;
    if(!UndoReadFromDisk(blockUndo, CHECK_NONFATAL(active_chain[currentHeight]))) {
       return 0;
    }

    InvalidTx invalidTx;
    chainman.ActiveChainstate().psignedblocktree->GetInvalidTx(currentHeight,invalidTx); 
    CAmount totalFee = 0; 

    LogPrintf("testing 5 4\n");
    for (size_t i = 0; i < prevblock.vtx.size(); ++i) {
        const CTransactionRef& tx = prevblock.vtx.at(i);
        CAmount amt_total_in = 0;
        CAmount amt_total_out = 0;
        if(i > 0) {
            const CTxUndo* txundo = &blockUndo.vtxundo.at(i - 1);
            const bool have_undo = txundo != nullptr;
            if(invalidTx.invalidTxs.size()>0) {
                if(std::find(invalidTx.invalidTxs.begin(), invalidTx.invalidTxs.end(), tx->GetHash()) != invalidTx.invalidTxs.end()) {
                    continue;
                }
            }
            LogPrintf("testing 5 4 1\n");


            for (unsigned int ix = 0; ix < tx->vin.size(); ix++) {
                if (have_undo) {
                    const Coin& prev_coin = txundo->vprevout[ix];
                    LogPrintf("testing 5 4 1 1\n");
                    const CTxOut& prev_txout = prev_coin.out;
                    LogPrintf("testing 5 4 1 2\n");
                    if(!(tx->nVersion == TRANSACTION_COORDINATE_ASSET_CREATE_VERSION && prev_coin.IsBitAssetController())) {
                        amt_total_in += prev_txout.nValue;
                    } 
                }
                LogPrintf("testing 5 4 1 3\n");
            }
            LogPrintf("testing 5 4 2\n");
        }

        for (unsigned int ix = 0; ix < tx->vout.size(); ix++) {
            const CTxOut& txout = tx->vout[ix];
            if(tx->nVersion == TRANSACTION_COORDINATE_ASSET_CREATE_VERSION) {
                if(ix > 1) {
                    amt_total_out += txout.nValue;
                }
            } else if(tx->nVersion == TRANSACTION_PRECONF_VERSION) {
                if(ix > 0) {
                    amt_total_out += txout.nValue;
                }
            } else {
                amt_total_out += txout.nValue;
            }
        }
        LogPrintf("testing 5 4 3\n");
        totalFee = totalFee + (amt_total_in - amt_total_out);
    }

    LogPrintf("testing 5 5\n");
    if(!MoneyRange(totalFee)) {
        return 0;
    }

    return totalFee;

} 


CScript getMinerScript(ChainstateManager& chainman, int blockHeight) {
    LOCK(cs_main);
    CChain& active_chain = chainman.ActiveChain();
    int currentHeight = blockHeight - 3;
    CScript scriptPubKey;
    CBlock prevblock;
    if (!ReadBlockFromDisk(prevblock, CHECK_NONFATAL(active_chain[currentHeight]), Params().GetConsensus())) {
        return scriptPubKey;
    } 
    return prevblock.vtx[0]->vout[0].scriptPubKey;
}

CScript getFederationScript(ChainstateManager& chainman, int blockHeight) {
    LOCK(cs_main);
    CChain& active_chain = chainman.ActiveChain();
    int currentHeight = blockHeight - 3;
    CScript scriptPubKey;
    CBlock prevblock;
    if (!ReadBlockFromDisk(prevblock, CHECK_NONFATAL(active_chain[currentHeight]), Params().GetConsensus())) {
        return scriptPubKey;
    } 
    std::vector<unsigned char> wData(ParseHex(prevblock.currentKeys));
    const std::string prevWitnessHexStr(wData.begin(), wData.end());
    UniValue witnessVal(UniValue::VOBJ);
    if (!witnessVal.read(prevWitnessHexStr)) {
        LogPrintf("invalid witness params \n");
        return scriptPubKey;
    }
    const CTxDestination coinbaseScript = DecodeDestination(find_value(witnessVal.get_obj(), "current_address").get_str());
    return GetScriptForDestination(coinbaseScript);
}

CAmount getRefundForTx(const CTransactionRef& ptx, const SignedBlock& block, const CCoinsViewCache& inputs) {

    if(ptx->nVersion != TRANSACTION_PRECONF_VERSION) {
        return 0;
    }
    CAmount refund = 0;
    CAmount fee = block.currentFee * ::GetSerializeSize(ptx, SERIALIZE_TRANSACTION_NO_WITNESS);

    CAmount nValueIn = 0;
    for (unsigned int i = 0; i < ptx->vin.size(); ++i) {
        const COutPoint &prevout = ptx->vin[i].prevout;

        const Coin& coin = inputs.AccessCoin(prevout);
        if(!coin.IsSpent()) {
            continue;
        }

        if (!MoneyRange(coin.out.nValue) || !MoneyRange(nValueIn)) {
            continue;
        }
        nValueIn += coin.out.nValue;
    }
    const CAmount value_out = ptx->GetValueOut();
    if (nValueIn < value_out) { 
        return refund;
    }
    refund = (nValueIn - value_out) - fee;
    if(refund < 0) {
        return 0;
    }
    return refund;
}

