#include <iostream>
#include <uint256.h>
#include <serialize.h>
#include <validation.h>

template<typename Stream, typename CoordinatePreConfBlockType>
inline void UnserializeCoordinatePreConfBlock(CoordinatePreConfBlockType& blockData, Stream& s) {
    s >> blockData.fee;
    s >> blockData.txids;
    s >> blockData.refunds;
}

template<typename Stream, typename CoordinatePreConfBlockType>
inline void SerializeCoordinatePreConfBlock(const CoordinatePreConfBlockType& blockData, Stream& s) {
    s << blockData.fee;
    s << blockData.txids;
    s << blockData.refunds;
}

struct CoordinatePreConfBlock {
public:
    std::vector<uint256> txids; /*!< Preconf Mempool txid*/
    CAmount fee; /*!< final fee for signed block*/
    std::vector<CTxOut> refunds; /*!< Preconf Mempool txout*/

    template <typename Stream>
    inline void Serialize(Stream& s) const {
        SerializeCoordinatePreConfBlock(*this, s);
    }

    template <typename Stream>
    inline void Unserialize(Stream& s) {
        UnserializeCoordinatePreConfBlock(*this, s);
    }

    template <typename Stream>
    CoordinatePreConfBlock(deserialize_type, Stream& s) {
        Unserialize(s);
    }

    CoordinatePreConfBlock() {
        SetNull();
    }

    void SetNull()
    {
        fee = -1;
        txids.empty();
        refunds.empty();
    }
};

template<typename Stream, typename CoordinatePreConfVotesType>
inline void UnserializeCoordinatePreConfVotes(CoordinatePreConfVotesType& voteData, Stream& s) {
    s >> voteData.votes;
    s >> voteData.txid;
    s >> voteData.blockHeight;
}

template<typename Stream, typename CoordinatePreConfVotesType>
inline void SerializeCoordinatePreConfVotes(const CoordinatePreConfVotesType& voteData, Stream& s) {
    s << voteData.vote;
    s << voteData.txid;
    s << voteData.blockHeight;
}

struct CoordinatePreConfVotes {
public:
    uint256 txid; /*!< Preconf Mempool txid*/
    uint32_t votes; /*!< number votes received from anduro federation members */
    int32_t blockHeight; /*!< max block height where the preconf votes upto valid */

    template <typename Stream>
    inline void Serialize(Stream& s) const {
        SerializeCoordinatePreConfVotes(*this, s);
    }

    template <typename Stream>
    inline void Unserialize(Stream& s) {
        UnserializeCoordinatePreConfVotes(*this, s);
    }

    template <typename Stream>
    CoordinatePreConfVotes(deserialize_type, Stream& s) {
        Unserialize(s);
    }

    CoordinatePreConfVotes() {
        SetNull();
    }

    void SetNull()
    {
        votes = 0;
        txid.SetNull();
        blockHeight = -1;
    }
};

template<typename Stream, typename CoordinatePreConfSigType>
inline void UnserializeCoordinatePreConfSig(CoordinatePreConfSigType& assetData, Stream& s) {
    s >> assetData.txid;
    s >> assetData.witness;
    s >> assetData.anduroPos;
    s >> assetData.blockHeight;
    s >> assetData.minedBlockHeight;
    s >> assetData.isBroadcasted;
    s >> assetData.peerList;
}

template<typename Stream, typename CoordinatePreConfSigType>
inline void SerializeCoordinatePreConfSig(const CoordinatePreConfSigType& assetData, Stream& s) {
    s << assetData.txid;
    s << assetData.witness;
    s << assetData.anduroPos;
    s << assetData.blockHeight;
    s << assetData.minedBlockHeight;
    s << assetData.isBroadcasted;
    s << assetData.peerList;
}

struct CoordinatePreConfSig {
public:
    uint256 txid; /*!< Preconf Mempool txid*/
    int32_t blockHeight; /*!< max block height where the preconf signature upto valid */
    int32_t minedBlockHeight; /*!< federation public key reference */
    int32_t anduroPos;  /*!< federation  position in previous header */
    std::string witness; /*!< federation signature against the txid and utctime */
    std::vector<int64_t> peerList;
    bool isBroadcasted; /*!< identify that it was broadcasted to the peers */

    template <typename Stream>
    inline void Serialize(Stream& s) const {
        SerializeCoordinatePreConfSig(*this, s);
    }

    template <typename Stream>
    inline void Unserialize(Stream& s) {
        UnserializeCoordinatePreConfSig(*this, s);
    }

    template <typename Stream>
    CoordinatePreConfSig(deserialize_type, Stream& s) {
        Unserialize(s);
    }

    CoordinatePreConfSig() {
        SetNull();
    }

    void SetNull()
    {
        peerList.empty();
        utcTime = -1;
        anduroPos = -1;
        blockHeight = -1;
        minedBlockHeight = -1;
        txid.SetNull();
        witness = "";
        isBroadcasted = false;
    }
};


/**
 * This function get mempool entry based on preconf signature
 */
CoordinatePreConfBlock getNextPreConfSigList(ChainstateManager& chainman);

/**
 * This function include preconf witness from anduro
 * @param[in] preconf hold three block presigned signature.
 * @param[in] chainman  used to find previous blocks based on active chain state to valid preconf signatures
 */
bool includePreConfSigWitness(std::vector<CoordinatePreConfSig> preconf, ChainstateManager& chainman);

/**
 * This function used to remove preconf signature afer included to block
 */
void removePreConfSigWitness(ChainstateManager& chainman);

/**
 * This is the function which used to get unbroadcasted preconfirmation signatures
 */
std::vector<CoordinatePreConfSig> getUnBroadcastedPreConfSig();

/**
 * This is the function which used to get all preconfirmation signatures
 */
std::vector<CoordinatePreConfSig> getPreConfSig();

/**
 * This is the function which used change status for broadcasted preconf
 */
void updateBroadcastedPreConf(CoordinatePreConfSig& preconfItem, int64_t peerId);

/**
 * This is the function which used to sorted preconf based on vote
 */
std::vector<CoordinatePreConfVotes> getSortedPreConf(int thresold);

/**
 * Calculate next block preconf fee based on selected list
*/
CAmount nextBlockFee(CTxMemPool& preconf_pool, std::vector<CoordinatePreConfVotes> preconfVoteList);

/**
 * Calculate next block refunds
*/
CoordinatePreConfBlock prepareRefunds(CTxMemPool& preconf_pool, std::vector<CoordinatePreConfVotes> preconfVoteList, CAmount finalFee);

/**
 * This is the function which used to get all preconfirmation signatures
 */
std::vector<CoordinatePreConfSig> getPreConfSig();

/**
 * This is the function which used to get all preconfirmation votes
 */
std::vector<CoordinatePreConfVotes> getPreConfVotes();

/**
 * This function prepare federation output for preconf
 * @param[in] txid preconf transaction id
 * @param[in] blockHeight  block height 
 * @param[out] preconfVoteObj  preconf vout object
 */
bool getPreConfVote(uint256 txid, int32_t blockHeight, CoordinatePreConfVotes* preconfVoteObj);

/**
 * This function prepare federation output for preconf
 * @param[in] federationFee federation fee for preconf transaction
 * @param[in] chainman  used to find previous blocks based on active chain state to valid preconf signatures
 */
CTxOut getFederationOutForFee(ChainstateManager& chainman, CAmount federationFee);

/**
 * This function remove old federation signature for preconf
 * @param[in] minFee next federation min fee for preconf transaction
 * @param[in] chainman  used to find previous blocks based on active chain state to valid preconf signatures
 */
void removePreConfWitness(ChainstateManager& chainman, int32_t nHeight);

/**
 * This function return preconf minimum fee
 */
CAmount getPreConfMinFee();