
#include <iostream>
#include <uint256.h>
#include <serialize.h>

/**
 * Asset Basic registry structure
*/
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
    uint32_t nID;
    uint32_t assetType;
    std::string strTicker;
    std::string strHeadline;
    uint256 payload;
    uint256 txid;
    int64_t nSupply;
    std::string strController;
    std::string strOwner;

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
        nID = -1;
        assetType = -1;
        strTicker="";
        strHeadline="";
        payload.SetNull();
        txid.SetNull();
        nSupply=-1;
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
    uint32_t nID;
    uint256 txid;
    std::string dataHex;

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
        nID = -1;
        txid.SetNull();
        dataHex="";
    }

};

