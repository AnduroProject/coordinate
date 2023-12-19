#include <chroma/chroma_mempool_entry.h>


std::vector<ChromaMempoolEntry> chromaMempoolEntry;

bool getMempoolAsset(uint256 txid, uint32_t voutIn, ChromaMempoolEntry* assetMempoolObj) {
    auto it = std::find_if(chromaMempoolEntry.begin(), chromaMempoolEntry.end(), 
                       [txid,voutIn] (const ChromaMempoolEntry& d) { 
                          return d.txid == txid && d.vout == voutIn; 
                       });
    assetMempoolObj->assetID= it->assetID;
    assetMempoolObj->txid= it->txid;
    assetMempoolObj->vout= it->vout;
    assetMempoolObj->nValue= it->nValue;
    return assetMempoolObj->assetID == 0 ? false : true;
}

int findMempoolAssetByTxid(uint256 txid) {
    auto it = std::find_if(chromaMempoolEntry.begin(), chromaMempoolEntry.end(), 
                       [txid] (const ChromaMempoolEntry& d) { 
                          return d.txid == txid; 
                       });
    if (it == chromaMempoolEntry.end()) {
       return -1;
    } else {
       return it - chromaMempoolEntry.begin();
    }
}

bool addMempoolAsset(ChromaMempoolEntry& assetMempoolObj) {
    chromaMempoolEntry.push_back(assetMempoolObj);
}

bool removeMempoolAsset(uint256 txidIn) {
    int indexToRemove =  findMempoolAssetByTxid(txidIn);
    if(indexToRemove != -1)  {
        chromaMempoolEntry.erase(chromaMempoolEntry.begin() + indexToRemove);
    }
}

void includeMempoolAsset(const CTransaction& tx, Chainstate& m_active_chainstate) {
    uint32_t currentAssetID = 0;
    CAmount amountAssetIn = 0;
    bool has_asset_amount = getAssetWithAmount(tx,m_active_chainstate,amountAssetIn, currentAssetID);
    if(has_asset_amount) {
        CAmount amountAssetOut = 0;
        for (unsigned int i = 0; i < tx.vout.size(); i++) {
            if(amountAssetOut == amountAssetIn) {
                break;
            }
            ChromaMempoolEntry assetMempoolObj;
            assetMempoolObj.assetID = currentAssetID;
            assetMempoolObj.txid = tx.GetHash();
            assetMempoolObj.vout = i;
            assetMempoolObj.nValue = tx.vout[i].nValue;
            addMempoolAsset(assetMempoolObj);
            amountAssetOut = amountAssetOut + tx.vout[i].nValue;
        }
    }
}

bool getAssetWithAmount(const CTransaction& tx, Chainstate& m_active_chainstate, CAmount& amountAssetIn, uint32_t& currentAssetID) {
    CCoinsViewCache& mapInputs = m_active_chainstate.CoinsTip();
    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        uint32_t nAssetID = 0;
        bool fBitAsset = false;
        bool fBitAssetControl = false;
        Coin coin;
        bool is_asset = mapInputs.getAssetCoin(tx.vin[i].prevout,fBitAsset,fBitAssetControl,nAssetID, &coin);
        if(!is_asset) {
            ChromaMempoolEntry* assetMempoolObj;
            bool is_mempool_asset = getMempoolAsset(tx.vin[i].prevout.hash,tx.vin[i].prevout.n, assetMempoolObj);
            nAssetID = assetMempoolObj->assetID;
            if(is_mempool_asset) {
                amountAssetIn = amountAssetIn + assetMempoolObj->nValue;
            }
        } else {
            if(fBitAssetControl) {
                currentAssetID = 0;
                break;
            }
            amountAssetIn = amountAssetIn + coin.out.nValue;
        }

        if(!nAssetID) {
           break;
        } 
        currentAssetID = nAssetID;
    }

    return currentAssetID > 0 ? true : false;
}

int getAssetOutputCount(const CTransaction& tx, Chainstate& m_active_chainstate) {
    if(tx.nVersion == TRANSACTION_CHROMAASSET_CREATE_VERSION) {
        return 2;
    }
    if(tx.nVersion == TRANSACTION_CHROMAASSET_TRANSFER_VERSION) {
        uint32_t totalOutputs = 0;
        uint32_t currentAssetID = 0;
        CAmount amountAssetIn = 0;
        bool has_asset_amount = getAssetWithAmount(tx,m_active_chainstate,amountAssetIn, currentAssetID);
        if(has_asset_amount) {
            CAmount amountAssetOut = 0;
            for (unsigned int i = 0; i < tx.vout.size(); i++) {
                if(amountAssetOut == amountAssetIn) {
                    break;
                }
                totalOutputs = totalOutputs + 1;
                amountAssetOut = amountAssetOut + tx.vout[i].nValue;
            }
        }
        return totalOutputs;
    }

    return 0;
}
 
std::vector<ChromaMempoolEntry> getMempoolAssets() {
   return chromaMempoolEntry;
}