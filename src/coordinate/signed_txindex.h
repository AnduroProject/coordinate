#ifndef BITCOIN_SIGNEDTXINDEX_H
#define BITCOIN_SIGNEDTXINDEX_H

#include <serialize.h>
#include <uint256.h>

class SignedTxindex {
    public:
        uint32_t pos;
        int blockIndex;
        uint256 signedBlockHash;

        SignedTxindex() {
         SetNull();
        }

        SERIALIZE_METHODS(SignedTxindex, obj) { READWRITE(obj.pos, obj.blockIndex, obj.signedBlockHash);}
        void SetNull()
        {
            pos = 0;
            blockIndex = 0;
            signedBlockHash.SetNull();
        }
};

#endif // BITCOIN_SIGNEDTXINDEX_H