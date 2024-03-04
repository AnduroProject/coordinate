#include <coordinate/coordinate_preconf.h>
#include <core_io.h>
#include <logging.h>
#include <univalue.h>
#include <node/blockstorage.h>
#include <anduro_validator.h>
#include <validation.h>
#include <consensus/merkle.h>

using node::BlockManager;
using node::ReadBlockFromDisk;

std::vector<CoordinatePreConfSig> coordinatePreConfSig;

// temporary minfee
CAmount preconfMinFee = 5;

CoordinatePreConfBlock getNextPreConfSigList(ChainstateManager& chainman) {
    uint64_t signedBlockHeight = 0;
    chainman.ActiveChainstate().psignedblocktree->GetLastSignedBlockID(signedBlockHeight);

    CChain& active_chain = chainman.ActiveChain();
    int blockindex = active_chain.Height();
    CBlock block;
    if (!ReadBlockFromDisk(block, CHECK_NONFATAL(active_chain[blockindex]), Params().GetConsensus())) {
        LogPrintf("Error reading block from disk at index %d\n", CHECK_NONFATAL(active_chain[blockindex])->GetBlockHash().ToString());
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
        if(coordinatePreConfSigtem.blockHeight != signedBlockHeight) {
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
        if(coordinatePreConfSigtem.blockHeight != signedBlockHeight) {
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
    if(preconf[0].blockHeight <= signedBlockHeight) {
        LogPrintf("preconf transaction sig has old block height \n");
        return false;
    }

    int blockindex = preconf[0].minedBlockHeight;

    // check list already exist array
    auto it = std::find_if(coordinatePreConfSig.begin(), coordinatePreConfSig.end(), 
                [signedBlockHeight] (const CoordinatePreConfSig& d) { 
                    return d.blockHeight == signedBlockHeight;
                });

    if (it != coordinatePreConfSig.end()) {
         LogPrintf("preconf transaction already exist \n");
        return false;
    }

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

    if(!validateAnduroSignature(preconf[0].witness,messages.write(),block.currentKeys)) {
       return false;
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
 * This function prepare federation output for preconf
 */
CTxOut getFederationOutForFee(ChainstateManager& chainman, CAmount federationFee) {
    LOCK(cs_main);
    CChain& active_chain = chainman.ActiveChain();
    int blockindex = active_chain.Height();
    CBlock block;
    CTxOut federationOut;
    if (!ReadBlockFromDisk(block, CHECK_NONFATAL(active_chain[blockindex]), Params().GetConsensus())) {
        LogPrintf("Error reading block from disk at index %d\n", CHECK_NONFATAL(active_chain[blockindex])->GetBlockHash().ToString());
        return federationOut;
    }

    std::vector<unsigned char> wData(ParseHex(block.currentKeys));
    const std::string prevWitnessHexStr(wData.begin(), wData.end());
    UniValue witnessVal(UniValue::VOBJ);
    if (!witnessVal.read(prevWitnessHexStr)) {
        LogPrintf("invalid witness params \n");
        return federationOut;
    }
    const CTxDestination coinbaseScript = DecodeDestination(find_value(witnessVal.get_obj(), "current_address").get_str());
    const CScript scriptPubKey = GetScriptForDestination(coinbaseScript);

    federationOut.nValue = federationFee;
    federationOut.scriptPubKey = scriptPubKey;

    return federationOut;
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
        LogPrintf("Signed block witness not exist \n");
        return nullptr;
    }

    LogPrintf("CreateNewSignedBlock 1\n");

    SignedBlock prevBlock;
    chainman.ActiveChainstate().psignedblocktree->GetLastSignedBlockID(nIDLast);
    if(nIDLast > 0) {
        chainman.ActiveChainstate().psignedblocktree->GetSignedBlock(nIDLast,prevBlock);
        block->hashPrevSignedBlock = prevBlock.GetHash();
    }
    LogPrintf("CreateNewSignedBlock 2\n");
    CTxMemPool& preconf_pool{*chainman.ActiveChainstate().GetPreConfMempool()};

    uint64_t nHeight = nIDLast + 1;
    block->currentFee = preconfList.fee;
    block->nHeight = nHeight;
    block->nTime = nTime;
    block->blockIndex = chainman.ActiveHeight();
    block->vtx.resize(preconfList.txids.size() + 1);


    LogPrintf("CreateNewSignedBlock 3\n");

    unsigned int i = 1;
    std::vector<CTxOut> coinBaseOuts;

    std::vector<unsigned char> witnessData = ParseHex(preconfList.witness);
    CTxOut witnessOut(0, CScript() << OP_RETURN << witnessData);
    coinBaseOuts.push_back(witnessOut);


    CAmount preConfMinerFee = 0;
    CAmount preConfFederationFee = 0;
    LogPrintf("CreateNewSignedBlock 4\n");
    for (const uint256& hash : preconfList.txids) {
        TxMempoolInfo info = preconf_pool.info(GenTxid::Txid(hash));
        if(!info.tx) {
           LogPrintf("Signed block invalid tx \n");
           return nullptr;
        }
        CAmount totalFee = preconfList.fee * info.vsize;
        LogPrintf("pre conf total fee for tx %i \n", totalFee);
        CAmount minerFee = std::ceil(totalFee * 0.8);
        LogPrintf("pre conf miner fee for tx %i \n", minerFee);
        preConfMinerFee = preConfMinerFee + minerFee;
        preConfFederationFee = preConfFederationFee + (totalFee - minerFee); 
        CTxOut refund(info.fee - totalFee, info.tx->vout[0].scriptPubKey);
        coinBaseOuts.push_back(refund);

        block->vtx[i] = info.tx;
        i = i + 1;
    }
    LogPrintf("CreateNewSignedBlock 5\n");
    if(preConfMinerFee>0) {
        LogPrintf("pre conf miner fee %i \n", preConfMinerFee);
        LogPrintf("pre conf fed fee %i \n", preConfFederationFee);
        std::vector<unsigned char> minerShare = ParseHex("Miner");
        CTxOut minerShareOut(preConfMinerFee, CScript() << OP_RETURN << minerShare);
        coinBaseOuts.push_back(minerShareOut);

        std::vector<unsigned char> anduroShare = ParseHex("Anduro");
        CTxOut anduroShareOut(preConfFederationFee, CScript() << OP_RETURN << anduroShare);
        coinBaseOuts.push_back(anduroShareOut);
    }

    std::vector<unsigned char> witnessMerkleData = ParseHex(SignedBlockWitnessMerkleRoot(*block).ToString());
    CTxOut witnessMerkleOut(0, CScript() << OP_RETURN << witnessMerkleData);
    coinBaseOuts.push_back(witnessMerkleOut);

    LogPrintf("CreateNewSignedBlock 6\n");
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

    LogPrintf("CreateNewSignedBlock 7\n");

    return std::move(pblocktemplate);
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