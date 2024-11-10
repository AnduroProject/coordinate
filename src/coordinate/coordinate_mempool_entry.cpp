#include <coordinate/coordinate_mempool_entry.h>
#include <anduro_validator.h>

/**
 * This is the function which used to get asset total amount
 */
bool getAssetWithAmount(const CTransaction& tx, Chainstate& m_active_chainstate, CAmount& amountAssetIn, uint32_t& currentAssetID) {
    CCoinsViewCache& mapInputs = m_active_chainstate.CoinsTip();
    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        uint32_t nAssetID = 0;
        bool fBitAsset = false;
        bool fBitAssetControl = false;
        Coin coin;
        if(mapInputs.getAssetCoin(tx.vin[i].prevout,fBitAsset,fBitAssetControl,nAssetID, &coin)) {
            if(fBitAssetControl) {
                currentAssetID = 0;
                break;
            }
            if(fBitAsset) {
                amountAssetIn = amountAssetIn + coin.out.nValue;
            }
        }

        if(!nAssetID) {
           break;
        } 
        currentAssetID = nAssetID;
    }

    return currentAssetID > 0 ? true : false;
}

/**
 * This is the function which get asset ouput information for particular transaction 
 */
int getAssetOutputCount(const CTransaction& tx, Chainstate& m_active_chainstate) {
    if(tx.nVersion == TRANSACTION_COORDINATE_ASSET_CREATE_VERSION) {
        return 2;
    }
    if(tx.nVersion == TRANSACTION_COORDINATE_ASSET_TRANSFER_VERSION) {
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