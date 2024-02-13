#include <coordinate/coordinate_preconf.h>
#include <core_io.h>
#include <logging.h>
#include <univalue.h>
#include <node/blockstorage.h>
#include <anduro_validator.h>
#include <validation.h>

using node::BlockManager;
using node::ReadBlockFromDisk;

std::vector<CoordinatePreConfSig> coordinatePreConfSig;

std::vector<CoordinatePreConfVotes> coordinatePreConfVotes;

// temporary minfee
CAmount preconfMinFee;

CoordinatePreConfBlock getNextPreConfSigList(ChainstateManager& chainman) {
    CChain& active_chain = chainman.ActiveChain();
    int blockindex = active_chain.Height();
    CBlock block;
    if (!ReadBlockFromDisk(block, CHECK_NONFATAL(active_chain[blockindex]), Params().GetConsensus())) {
        LogPrintf("Error reading block from disk at index %d\n", CHECK_NONFATAL(active_chain[blockindex])->GetBlockHash().ToString());
        CoordinatePreConfBlock result;
        return result;
    }

    std::vector<unsigned char> wData(ParseHex(block.currentKeys));
    const std::string prevWitnessHexStr(wData.begin(), wData.end());
    UniValue witnessVal(UniValue::VOBJ);
    if (!witnessVal.read(prevWitnessHexStr)) {
        LogPrintf("invalid witness params \n");
        CoordinatePreConfBlock result;
        return result;
    }

    std::vector<std::string> allKeysArray;
    const auto allKeysArrayRequest = find_value(witnessVal.get_obj(), "allkeys").get_array();

    int thresold =  std::ceil(allKeysArray.size() * 0.6);
    std::vector<CoordinatePreConfVotes> preconfVoteList = getSortedPreConf(thresold);

    CTxMemPool& preconf_pool{*chainman.ActiveChainstate().GetPreConfMempool()};

    CAmount finalFee = nextBlockFee(preconf_pool, preconfVoteList);
    
    return prepareRefunds(preconf_pool,preconfVoteList,finalFee);
}

CAmount nextBlockFee(CTxMemPool& preconf_pool, std::vector<CoordinatePreConfVotes> preconfVoteList) {
    CAmount finalFee = 0;
    for (const CoordinatePreConfVotes& preconfVoteListItem : preconfVoteList) {
        CTransactionRef tx = preconf_pool.get(preconfVoteListItem.txid);
        if(finalFee == 0 || finalFee > tx->vout[0].nValue) {
            finalFee = tx->vout[0].nValue;
        }
    } 
    return finalFee;
}

CoordinatePreConfBlock prepareRefunds(CTxMemPool& preconf_pool, std::vector<CoordinatePreConfVotes> preconfVoteList, CAmount finalFee) {
    std::vector<CTxOut> txouts;
    std::vector<uint256> txids;
    for (const CoordinatePreConfVotes& preconfVoteListItem : preconfVoteList) {
        TxMempoolInfo info = preconf_pool.info(GenTxid::Txid(preconfVoteListItem.txid));
        if(finalFee > info.tx->vout[0].nValue) { 
            CAmount refundAmount =  info.fee - (info.vsize * finalFee);
            txouts.push_back(CTxOut(refundAmount,info.tx->vout[0].scriptPubKey));
        }
        txids.push_back(preconfVoteListItem.txid);
    }

    CoordinatePreConfBlock result;
    result.txids = txids;
    result.refunds = txouts;
    result.fee = finalFee;
    return result;
}

bool includePreConfSigWitness(std::vector<CoordinatePreConfSig> preconf, ChainstateManager& chainman) {
    LOCK(cs_main);
    CChain& active_chain = chainman.ActiveChain();
    CTxMemPool& preconf_pool{*chainman.ActiveChainstate().GetPreConfMempool()};
    LOCK(preconf_pool.cs);
    int blockindex = active_chain.Height();

    // check preconf data is empty
    if(preconf.size()==0) {
        return false;
    }
    // check preconf sign block height is valid
    if(preconf.size()>0) {
        if(preconf[0].blockHeight <= blockindex) {
           return false;
        }
    }

    // check list already exist array
    int32_t anduroPos = preconf[0].anduroPos;
    uint256 txid =  preconf[0].txid;
    auto it = std::find_if(coordinatePreConfSig.begin(), coordinatePreConfSig.end(), 
                [blockindex,anduroPos,txid] (const CoordinatePreConfSig& d) { 
                    return d.blockHeight > blockindex && d.anduroPos == anduroPos && d.txid == txid;
                });

    if (it == coordinatePreConfSig.end()) {
        return false;
    }

    // check txid exist in preconf mempool
    for (const CoordinatePreConfSig& coordinatePreConfSigItem : coordinatePreConfSig) {
       if(!preconf_pool.exists(GenTxid::Txid(coordinatePreConfSigItem.txid))) {
            return false;
       }
    }

    //check signature is valid and include to queue
    UniValue messages(UniValue::VARR);
    for (const CoordinatePreConfSig& coordinatePreConfSigItem : coordinatePreConfSig) {
       if(preconf_pool.exists(GenTxid::Txid(coordinatePreConfSigItem.txid))) {
            UniValue message(UniValue::VOBJ);
            message.pushKV("txid", coordinatePreConfSigItem.txid.ToString());
            message.pushKV("time", coordinatePreConfSigItem.utcTime);
            message.pushKV("height", coordinatePreConfSigItem.blockHeight);
            messages.push_back(message);
       }
    }

    // get block to find the eligible anduro keys to be signed on presigned block
    CBlock block;
    if (!ReadBlockFromDisk(block, CHECK_NONFATAL(active_chain[blockindex]), Params().GetConsensus())) {
        LogPrintf("Error reading block from disk at index %d\n", CHECK_NONFATAL(active_chain[blockindex])->GetBlockHash().ToString());
    }

    if(!validatePreConfSignature(preconf[0].witness,messages.write(),block.currentKeys,preconf[0].anduroPos)) {
       return false;
    }

    for (const CoordinatePreConfSig& preconfItem : preconf) {
        CoordinatePreConfVotes preconfVoteObj;
        bool is_vote_exist = getPreConfVote(preconfItem.txid, preconfItem.blockHeight, &preconfVoteObj);
        if(is_vote_exist) {
            preconfVoteObj.votes = preconfVoteObj.votes + 1;
        } else {
            preconfVoteObj.blockHeight = preconfItem.blockHeight;
            preconfVoteObj.txid = preconfItem.txid;
            preconfVoteObj.votes = 1;
            coordinatePreConfVotes.push_back(preconfVoteObj);
        }
        coordinatePreConfSig.push_back(preconfItem);
    }
}

void removePreConfWitness(ChainstateManager& chainman, CAmount minFee) {
    LOCK(cs_main);
    CChain& active_chain = chainman.ActiveChain();
    int blockindex = active_chain.Height();
    auto sigit = std::find_if(coordinatePreConfSig.begin(), coordinatePreConfSig.end(), 
                    [blockindex] (const CoordinatePreConfSig& d) { 
                        return d.blockHeight < blockindex;
                    });
    if (sigit != coordinatePreConfSig.end()) {
        int indexToRemove = sigit - coordinatePreConfSig.begin() ;
        coordinatePreConfSig.erase(coordinatePreConfSig.begin() + indexToRemove);
    }

    auto it = std::find_if(coordinatePreConfVotes.begin(), coordinatePreConfVotes.end(), 
                    [blockindex] (const CoordinatePreConfVotes& d) { 
                        return d.blockHeight < blockindex;
                    });
    if (it != coordinatePreConfVotes.end()) {
        int indexToRemove = it - coordinatePreConfVotes.begin() ;
        coordinatePreConfVotes.erase(coordinatePreConfVotes.begin() + indexToRemove);
    }
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
 * This is the function which used to sorted preconf based on vote
 */
std::vector<CoordinatePreConfVotes> getSortedPreConf(int thresold) {
    std::vector<CoordinatePreConfVotes> votes;
    for (CoordinatePreConfVotes coordinatePreConfVotesItem : coordinatePreConfVotes) {
        if(coordinatePreConfVotesItem.votes >= thresold) {
            votes.push_back(coordinatePreConfVotesItem);
        }
    }
    return votes;
}

/**
 * This is the function which used to get all preconfirmation signatures
 */
std::vector<CoordinatePreConfSig> getPreConfSig() {
   return coordinatePreConfSig;
}

/**
 * This is the function which used to get all preconfirmation votes
 */
std::vector<CoordinatePreConfVotes> getPreConfVotes() {
   return coordinatePreConfVotes;
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
bool getPreConfVote(uint256 txid, int32_t blockHeight, CoordinatePreConfVotes* preconfVoteObj) {
    auto it = std::find_if(coordinatePreConfVotes.begin(), coordinatePreConfVotes.end(), 
                       [txid,blockHeight] (const CoordinatePreConfVotes& d) { 
                          return d.txid == txid && d.blockHeight== blockHeight; 
                       });
    if (it == coordinatePreConfVotes.end()) return false;

    *preconfVoteObj = std::move(*it);
    return preconfVoteObj->votes == 0 ? false : true;
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