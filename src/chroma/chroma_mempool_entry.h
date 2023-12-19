#include <iostream>
#include <uint256.h>
#include <serialize.h>
#include <consensus/amount.h>
#include <primitives/transaction.h>
#include <validation.h>

/**
 * Chroma mempool entry registry structure
*/
template<typename Stream, typename ChromaMempoolEntryType>
inline void UnserializeChromaMempoolEntry(ChromaMempoolEntryType& assetData, Stream& s) {
    s >> assetData.assetID;
    s >> assetData.txid;
    s >> assetData.vout;
    s >> assetData.nValue;
}

template<typename Stream, typename ChromaMempoolEntryType>
inline void SerializeChromaMempoolEntry(const ChromaMempoolEntryType& assetData, Stream& s) {
    s << assetData.assetID;
    s << assetData.txid;
    s >> assetData.vout;
    s >> assetData.nValue;
}

struct ChromaMempoolEntry {
public:
    uint32_t assetID; /*!< Asset unique number */
    uint256 txid;  /*!< Asset Mempool txid*/
    uint32_t vout;  /*!< Asset Transaction vout*/
    CAmount nValue;  /*!< Asset Transaction value*/

    template <typename Stream>
    inline void Serialize(Stream& s) const {
        SerializeChromaMempoolEntry(*this, s);
    }

    template <typename Stream>
    inline void Unserialize(Stream& s) {
        UnserializeChromaMempoolEntry(*this, s);
    }

    template <typename Stream>
    ChromaMempoolEntry(deserialize_type, Stream& s) {
        Unserialize(s);
    }

    ChromaMempoolEntry() {
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


bool getMempoolAsset(uint256 txid, uint32_t voutIn, ChromaMempoolEntry* assetMempoolObj);
int findMempoolAssetByTxid(uint256 txid);
bool getAssetWithAmount(const CTransaction& tx, Chainstate& m_active_chainstate, CAmount& amountAssetIn, uint32_t& currentAssetID);
bool addMempoolAsset(ChromaMempoolEntry& assetMempoolObj);
bool removeMempoolAsset(uint256 txidIn);
void includeMempoolAsset(const CTransaction& tx, Chainstate& m_active_chainstate);
int getAssetOutputCount(const CTransaction& tx, Chainstate& m_active_chainstate);
std::vector<ChromaMempoolEntry> getMempoolAssets();