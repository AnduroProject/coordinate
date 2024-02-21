#ifndef BITCOIN_SIGNEDTXINDEX_H
#define BITCOIN_SIGNEDTXINDEX_H

#include <serialize.h>
#include <uint256.h>

class SignedTxindex {
    public:
        uint32_t pos;
        uint64_t nHeight;

        SignedTxindex() {
         SetNull();
        }

        SERIALIZE_METHODS(SignedTxindex, obj) { READWRITE(obj.pos, obj.nHeight);}

        void SetNull()
        {
            pos = 0;
            nHeight = 0;
        }
};

#endif // BITCOIN_SIGNEDTXINDEX_H