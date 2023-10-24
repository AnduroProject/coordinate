
#include <iostream>
#include <uint256.h>
#include <serialize.h>

template<typename Stream, typename ChromaType>
inline void UnserializeAsset(ChromaType& assetData, Stream& s) {
    s >> assetData.nID;
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
        strTicker="";
        strHeadline="";
        payload.SetNull();
        txid.SetNull();
        nSupply=-1;
        strController="";
        strOwner="";
    }

};
