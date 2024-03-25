#include <iostream>
#include <uint256.h>
#include <serialize.h>
#include <validation.h>

template<typename Stream, typename CoordinatePreConfBlockType>
inline void UnserializeCoordinatePreConfBlock(CoordinatePreConfBlockType& blockData, Stream& s) {
    s >> blockData.fee;
    s >> blockData.txids;
    s >> blockData.refunds;
    s >> blockData.witness;
}

template<typename Stream, typename CoordinatePreConfBlockType>
inline void SerializeCoordinatePreConfBlock(const CoordinatePreConfBlockType& blockData, Stream& s) {
    s << blockData.fee;
    s << blockData.txids;
    s << blockData.refunds;
    s << blockData.witness;
}

struct CoordinatePreConfBlock {
public:
    std::vector<uint256> txids; /*!< Preconf Mempool txid*/
    CAmount fee; /*!< final fee for signed block*/
    std::vector<CTxOut> refunds; /*!< Preconf Mempool txout*/
    std::string witness;

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
        witness = "";
    }
};




template<typename Stream, typename CoordinatePreConfSigType>
inline void UnserializeCoordinatePreConfSig(CoordinatePreConfSigType& preconfData, Stream& s) {
    s >> preconfData.txid;
    s >> preconfData.blockHeight;
    s >> preconfData.minedBlockHeight;
    s >> preconfData.reserve;
    s >> preconfData.vsize;
    s >> preconfData.witness;
    s >> preconfData.isBroadcasted;
    s >> preconfData.peerList;
    s >> preconfData.federationKey;
    s >> preconfData.finalized;
}

template<typename Stream, typename CoordinatePreConfSigType>
inline void SerializeCoordinatePreConfSig(const CoordinatePreConfSigType& preconfData, Stream& s) {
    s << preconfData.txid;
    s << preconfData.blockHeight;
    s << preconfData.minedBlockHeight;
    s << preconfData.reserve;
    s << preconfData.vsize;
    s << preconfData.witness;
    s << preconfData.isBroadcasted;
    s << preconfData.peerList;
    s << preconfData.federationKey;
    s << preconfData.finalized;
}

struct CoordinatePreConfSig {
public:
    uint256 txid; /*!< Preconf Mempool txid*/
    int32_t blockHeight; /*!< max block height where the preconf signature upto valid */
    int32_t minedBlockHeight; /*!< federation public key reference in mined block height*/
    int32_t reserve; /*!< transaction reserve */
    int32_t vsize; /*!< transaction virtula size */
    std::string witness;
    std::string federationKey; /*!< federation public key */
    uint32_t finalized; /*!< 0 - not finalized by federation, 1 - finalized by federation */
    bool isBroadcasted; /*!< identify that it was broadcasted to the peers */
    std::vector<int64_t> peerList;

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
        blockHeight = -1;
        reserve = -1;
        vsize = -1;
        minedBlockHeight = -1;
        txid.SetNull();
        witness = "";
        isBroadcasted = false;
        peerList.clear();
        federationKey = "";
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
 * This function include preconf witness from anduro
 * @param[in] newFinalizedSignedBlocks hold signed block list from network
 * @param[in] chainman  used to find previous blocks based on active chain state to valid preconf signatures
 */
bool includePreConfBlockFromNetwork(std::vector<SignedBlock> newFinalizedSignedBlocks, ChainstateManager& chainman);

/**
 * This function used to remove preconf signature afer included to block
 */
void removePreConfSigWitness(ChainstateManager& chainman);

/**
 * This is the function which used to get unbroadcasted preconfirmation signatures
 */
std::vector<CoordinatePreConfSig> getUnBroadcastedPreConfSig();

/**
 * This is the function which used to get unbroadcasted preconfirmation signed block
 */
std::vector<SignedBlock> getUnBroadcastedPreConfSignedBlock();

/**
 * This is the function which used to get all preconfirmation signatures
 */
std::vector<CoordinatePreConfSig> getPreConfSig();

/**
 * This is the function which used change status for broadcasted preconf
 */
void updateBroadcastedPreConf(CoordinatePreConfSig& preconfItem, int64_t peerId);


/**
 * This is the function which used change status for broadcasted signed block
 */
void updateBroadcastedSignedBlock(SignedBlock& signedBlockItem, int64_t peerId);



/**
 * Calculate next block preconf fee based on selected list
*/
CAmount nextBlockFee(CTxMemPool& preconf_pool, uint64_t signedBlockHeight);

/**
 * Calculate next block refunds
*/
CoordinatePreConfBlock prepareRefunds(CTxMemPool& preconf_pool, CAmount finalFee, uint64_t signedBlockHeight);
/**
 * This is the function which used to get all preconfirmation signatures
 */
std::vector<CoordinatePreConfSig> getPreConfSig();

/**
 * This function remove old federation signature for preconf
 */
void removePreConfWitness();

/**
 * This function remove finalized preconf block after included in mined block
 */
void removePreConfFinalizedBlock(int blockHeight);

/**
 * This function return preconf minimum fee
 */
CAmount getPreConfMinFee();

/**
 * This function get new block template for signed block
 */
std::unique_ptr<SignedBlock> CreateNewSignedBlock(ChainstateManager& chainman, uint32_t nTime);

bool checkSignedBlock(const SignedBlock& block, ChainstateManager& chainman);

std::vector<uint256> getInvalidTx(ChainstateManager& chainman);

uint256 getReconsiledBlock(ChainstateManager& chainman);

CAmount getRefundForTx(const CTransactionRef& ptx, const SignedBlock& block, const CCoinsViewCache& inputs) ;

CAmount getPreconfFeeForBlock(ChainstateManager& chainman, int blockHeight);

CAmount getFeeForBlock(ChainstateManager& chainman, int blockHeight);

CScript getMinerScript(ChainstateManager& chainman, int blockHeight);

CScript getFederationScript(ChainstateManager& chainman, int blockHeight);

/**
 * This is the function which used to get all finalized signed block
 */
std::vector<SignedBlock> getFinalizedSignedBlocks();

CCoinsViewCache& getPreconfCoinView(ChainstateManager& chainman);

void insertNewSignedBlock(const SignedBlock& newFinalizedSignedBlock);