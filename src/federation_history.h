
#include <iostream>
#include <uint256.h>
#include <serialize.h>

template<typename Stream, typename PegOutHistoryType>
inline void UnserializePegOutHistory(PegOutHistoryType& historyData, Stream& s) {
    s >> historyData.blockHeight;
    s >> historyData.pegoutData;
    s >> historyData.pegoutWitness;
}

template<typename Stream, typename PegOutHistoryType>
inline void SerializePegOutHistory(const PegOutHistoryType& historyData, Stream& s) {
    s << historyData.pegoutData;
    s << historyData.pegoutWitness;
    s << historyData.blockHeight;

}


struct FederationPegOutHistory {
public:
    uint32_t blockHeight;
    std::string pegoutData;
    std::string pegoutWitness;

    explicit FederationPegOutHistory();

    template <typename Stream>
    inline void Serialize(Stream& s) const {
        SerializePegOutHistory(*this, s);
    }


    template <typename Stream>
    inline void Unserialize(Stream& s) {
        UnserializePegOutHistory(*this, s);
    }

    template <typename Stream>
    FederationPegOutHistory(deserialize_type, Stream& s) {
        Unserialize(s);
    }
};
