#include <iostream>
#include <uint256.h>
#include <serialize.h>
#include <consensus/amount.h>
#include <primitives/transaction.h>
#include <validation.h>


template<typename Stream, typename CoordinateMempoolEntryType>
inline void UnserializeCoordinateMempoolEntry(CoordinateMempoolEntryType& assetData, Stream& s) {
    s >> assetData.assetID;
    s >> assetData.txid;
    s >> assetData.vout;
    s >> assetData.nValue;
}

template<typename Stream, typename CoordinateMempoolEntryType>
inline void SerializeCoordinateMempoolEntry(const CoordinateMempoolEntryType& assetData, Stream& s) {
    s << assetData.assetID;
    s << assetData.txid;
    s << assetData.vout;
    s << assetData.nValue;
}


struct CoordinateMempoolEntry {
public:
    uint32_t assetID; /*!< Asset unique number */
    uint256 txid;  /*!< Asset Mempool txid*/
    int32_t vout;  /*!< Asset Mempool Transaction vout*/
    CAmount nValue;  /*!< Asset Mempool Transaction value*/

    template <typename Stream>
    inline void Serialize(Stream& s) const {
        SerializeCoordinateMempoolEntry(*this, s);
    }

    template <typename Stream>
    inline void Unserialize(Stream& s) {
        UnserializeCoordinateMempoolEntry(*this, s);
    }

    template <typename Stream>
    CoordinateMempoolEntry(deserialize_type, Stream& s) {
        Unserialize(s);
    }

    CoordinateMempoolEntry() {
        SetNull();
    }

    void SetNull()
    {
        assetID = 0;
        txid.SetNull();
        vout = -1;
        nValue = -1;
    }
};

/**
 * This is the function which used to get mempool asset information
 * @param[in] txid  txid used in transaction inputs
 * @param[in] voutIn  vout used in transaction inputs
 * @param[in] assetMempoolObj  Coordinate mempool entry that return back with value
 */
bool getMempoolAsset(uint256 txid, uint32_t voutIn, CoordinateMempoolEntry* assetMempoolObj);
/**
 * This is the function which used to get mempool asset information by txid
 * @param[in] txid  mempool transaction id
 * @param[in] vout  mempool transaction vout
 */
int findMempoolAssetByTxid(uint256 txid, int32_t voutIn);
/**
 * This is the function which used to get asset total amount
 * @param[in] tx  transaction information to find what is the amount asset included
 * @param[in] m_active_chainstate  active chain state information
 * @param[in] amountAssetIn  returns total asset amount used in transaction
 * @param[in] currentAssetID  returns current asset id
 */
bool getAssetWithAmount(const CTransaction& tx, Chainstate& m_active_chainstate, CAmount& amountAssetIn, uint32_t& currentAssetID);
/**
 * This is the function which remove all asset transaction based on txid
 * @param[in] tx  transaction, used to find asset ouputs
 */
void removeMempoolAsset(const CTransaction& tx);
/**
 * This is the function which include mempool asset
 * @param[in] tx  transaction, going to included in coordinate mempool entry
 * @param[in] m_active_chainstate  active chain state information
 */
void includeMempoolAsset(const CTransaction& tx, Chainstate& m_active_chainstate);
/**
 * This is the function which get asset ouput information for particular transaction 
 * @param[in] tx  transaction, used to find asset ouputs
 * @param[in] m_active_chainstate  active chain state information
 */
int getAssetOutputCount(const CTransaction& tx, Chainstate& m_active_chainstate);
/**
 * This is the function which get mempool asset information through rpc
 */
std::vector<CoordinateMempoolEntry> getMempoolAssets();