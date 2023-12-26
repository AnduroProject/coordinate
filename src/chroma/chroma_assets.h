#include <iostream>
#include <uint256.h>
#include <serialize.h>

template<typename Stream, typename ChromaType>
inline void UnserializeAsset(ChromaType& assetData, Stream& s) {
    s >> assetData.nID;
    s >> assetData.assetType;
    s >> assetData.strTicker;
    s >> assetData.strHeadline;
    s >> assetData.payload;
    s >> assetData.txid;
    s >> assetData.nSupply;
    s >> assetData.strController;
    s >> assetData.strOwner;
}

template<typename Stream, typename ChromaType>
inline void SerializeAsset(const ChromaType& assetData, Stream& s) {
    s << assetData.nID;
    s << assetData.assetType;
    s << assetData.strTicker;
    s << assetData.strHeadline;
    s << assetData.payload;
    s << assetData.txid;
    s << assetData.nSupply;
    s << assetData.strController;
    s << assetData.strOwner;
}

struct ChromaAsset {
public:
    uint32_t nID; /*!< Asset unique number */
    uint32_t assetType; /*!< Asset type - (e.g. 0 - tokens, 1 - nft, 2 - blob nft) */
    std::string strTicker; /*!< Asset symbol. */
    std::string strHeadline; /*!< Asset name. */
    uint256 payload; /*!< Asset sha256 id combination for all asset information with asset data. */
    uint256 txid; /*!< Asset gensis id when the asset is newly minted */
    uint64_t nSupply; /*!< Asset current supply */
    std::string strController; /*!< Asset controller details, which used to mint additionally */
    std::string strOwner; /*!< Asset owner at the time of creating asset */

    template <typename Stream>
    inline void Serialize(Stream& s) const {
        SerializeAsset(*this, s);
    }

    template <typename Stream>
    inline void Unserialize(Stream& s) {
        UnserializeAsset(*this, s);
    }

    template <typename Stream>
    ChromaAsset(deserialize_type, Stream& s) {
        Unserialize(s);
    }

    ChromaAsset() {
        SetNull();
    }

    void SetNull()
    {
        nID = 0;
        assetType = 0;
        strTicker="";
        strHeadline="";
        payload.SetNull();
        txid.SetNull();
        nSupply= 0;
        strController="";
        strOwner="";
    }
};

/**
 * Asset data registry structure
*/
template<typename Stream, typename ChromaDataType>
inline void UnserializeAssetData(ChromaDataType& assetData, Stream& s) {
    s >> assetData.nID;
    s >> assetData.txid;
    s >> assetData.dataHex;
}

template<typename Stream, typename ChromaDataType>
inline void SerializeAssetData(const ChromaDataType& assetData, Stream& s) {
    s << assetData.nID;
    s << assetData.txid;
    s << assetData.dataHex;
}

struct ChromaAssetData {
public:
    uint32_t nID; /*!< Asset unique number */
    uint256 txid;  /*!< Asset gensis id when the asset is newly minted or additionaly minted */
    uint256 blockHash;  /*!< block hash where Asset gensis id located*/
    std::string dataHex; /*!< Asset data field hold asset image data or url with additional asset attributes  */

    template <typename Stream>
    inline void Serialize(Stream& s) const {
        SerializeAssetData(*this, s);
    }

    template <typename Stream>
    inline void Unserialize(Stream& s) {
        UnserializeAssetData(*this, s);
    }

    template <typename Stream>
    ChromaAssetData(deserialize_type, Stream& s) {
        Unserialize(s);
    }

    ChromaAssetData() {
        SetNull();
    }

    void SetNull()
    {
        nID = 0;
        txid.SetNull();
        blockHash.SetNull();
        dataHex="";
    }
};


