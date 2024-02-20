#ifndef BITCOIN_SIGNEDBLOCK_H
#define BITCOIN_SIGNEDBLOCK_H

#include <serialize.h>
#include <uint256.h>
#include <util/time.h>
#include <consensus/amount.h>
#include <primitives/transaction.h>

class SignedBlock {
    public:
        int32_t nVersion;
        uint32_t nTime;
        uint64_t nHeight;
        uint64_t blockIndex;
        uint256 hashPrevSignedBlock;
        uint256 hashMerkleRoot;
        CAmount currentFee;
        std::vector<CTransactionRef> vtx;
        SignedBlock() {
         SetNull();
        }

        SERIALIZE_METHODS(SignedBlock, obj) { READWRITE(obj.nVersion, obj.nTime, obj.nHeight, obj.blockIndex, obj.hashPrevSignedBlock, obj.hashMerkleRoot, obj.currentFee, obj.vtx); }

        void SetNull()
        {
            nVersion = 0;
            nHeight = 0;
            nTime = 0;
            blockIndex= 0;
            hashPrevSignedBlock.SetNull();
            hashMerkleRoot.SetNull();
            currentFee = 0;
            vtx.empty();
        }

        uint256 GetHash() const;
};

#endif // BITCOIN_SIGNEDBLOCK_H