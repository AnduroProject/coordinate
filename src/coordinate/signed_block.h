#ifndef BITCOIN_SIGNEDBLOCK_H
#define BITCOIN_SIGNEDBLOCK_H

#include <serialize.h>
#include <uint256.h>
#include <util/time.h>
#include <consensus/amount.h>
#include <primitives/transaction.h>

class SignedBlockHeader {
    public:
        int32_t nVersion; /*!< signed block version*/
        uint32_t nTime; /*!< signed bock time */
        uint64_t nHeight; /*!< signed block number */
        uint64_t blockIndex; /*!< mined block number where federation witness verfied back to federation public keys */
        uint256 hashPrevSignedBlock; /*!< previous signed block */
        uint256 hashMerkleRoot; /*!< signed block merkle root */
        CAmount currentFee; /*!< current signed block fee*/

        SignedBlockHeader() {
           SetNull();
        }

        SERIALIZE_METHODS(SignedBlockHeader, obj) { READWRITE(obj.nVersion, obj.nTime, obj.nHeight, obj.blockIndex, obj.hashPrevSignedBlock, obj.hashMerkleRoot, obj.currentFee); }

        void SetNull() {
            nVersion = 0;
            nHeight = 0;
            nTime = 0;
            blockIndex= 0;
            hashPrevSignedBlock.SetNull();
            hashMerkleRoot.SetNull();
            currentFee = 0;
        }

        uint256 GetHash() const;
};

class SignedBlock : public SignedBlockHeader{
    public:
        std::vector<CTransactionRef> vtx; /*!< signed bock transaction list */
        SignedBlock() {
         SetNull();
        }

        SERIALIZE_METHODS(SignedBlock, obj) {  
            READWRITE(AsBase<SignedBlockHeader>(obj), obj.vtx);
        }

        void SetNull()
        {
            SignedBlockHeader::SetNull();
            vtx.clear();
        }
};


class SignedBlockPeer {
    public:
        uint256 hash; /*!< signed block hash */
        bool isBroadcasted; /*!< identify that it was broadcasted to the peers */
        std::vector<int64_t> peerList; /*!< node peer id infomrat that received the signed block through network */
        SignedBlockPeer() {
         SetNull();
        }

        SERIALIZE_METHODS(SignedBlockPeer, obj) { READWRITE(obj.hash, obj.isBroadcasted, obj.peerList); }

        void SetNull()
        {
            hash.SetNull();
            static_cast<void>(peerList.empty());
            isBroadcasted = false;
        }
};


#endif // BITCOIN_SIGNEDBLOCK_H
