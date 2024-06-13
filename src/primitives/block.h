// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PRIMITIVES_BLOCK_H
#define BITCOIN_PRIMITIVES_BLOCK_H

#include <primitives/transaction.h>
#include <primitives/pureheader.h>
#include <serialize.h>
#include <uint256.h>
#include <util/time.h>
#include <auxpow.h>
#include <coordinate/signed_block.h>

/** Nodes collect new transactions into a block, hash them into a hash tree,
 * and scan through nonce values to make the block's hash satisfy proof-of-work
 * requirements.  When they solve the proof-of-work, they broadcast the block
 * to everyone and the block is added to the block chain.  The first transaction
 * in the block is a special one that creates a new coin owned by the creator
 * of the block.
 */
class CBlockHeader : public CPureBlockHeader
{
public:
    // header
    std::shared_ptr<CAuxPow> auxpow;
    std::string currentKeys;
    int32_t currentIndex;


    CBlockHeader()
    {
        SetNull();
    }

    SERIALIZE_METHODS(CBlockHeader, obj)
    {
        READWRITE(AsBase<CPureBlockHeader>(obj));
        if (obj.IsAuxpow())
        {
            SER_READ(obj, obj.auxpow = std::make_shared<CAuxPow>());
            assert(obj.auxpow != nullptr);
            READWRITE(*obj.auxpow);
        } else
        {
            SER_READ(obj, obj.auxpow.reset());
        }
        READWRITE(obj.currentKeys);
        READWRITE(obj.currentIndex);
    }

    void SetNull()
    {
        CPureBlockHeader::SetNull();
        currentKeys = "";
        currentIndex = 0;
        auxpow.reset();
    }

    /**
     * Set the block's auxpow (or unset it).  This takes care of updating
     * the version accordingly.
     */
    void SetAuxpow (std::unique_ptr<CAuxPow> apow);
};


class CBlock : public CBlockHeader
{
public:
    // network and disk
    std::vector<CTransactionRef> vtx;
    std::vector<SignedBlock> preconfBlock;
    std::vector<uint256> invalidTx;
    uint256 reconsiliationBlock;

    // memory only
    mutable bool fChecked;

    CBlock()
    {
        SetNull();
    }

    CBlock(const CBlockHeader &header)
    {
        SetNull();
        *(static_cast<CBlockHeader*>(this)) = header;
    }

    SERIALIZE_METHODS(CBlock, obj)
    {
        READWRITE(AsBase<CBlockHeader>(obj), obj.vtx);
        READWRITE(obj.invalidTx);
        READWRITE(obj.preconfBlock);
        READWRITE(obj.reconsiliationBlock);
    }

    void SetNull()
    {
        CBlockHeader::SetNull();
        vtx.clear();
        invalidTx.clear();
        preconfBlock.clear();
        reconsiliationBlock.SetNull();
        fChecked = false;
    }

    CBlockHeader GetBlockHeader() const {
        CBlockHeader block;
        block.nVersion       = nVersion;
        block.hashPrevBlock  = hashPrevBlock;
        block.hashMerkleRoot = hashMerkleRoot;
        block.nTime          = nTime;
        block.nBits          = nBits;
        block.nNonce         = nNonce;
        block.auxpow         = auxpow;
        return block;
    }

    std::string ToString() const;
};

/** Describes a place in the block chain to another node such that if the
 * other node doesn't have the same branch, it can find a recent common trunk.
 * The further back it is, the further before the fork it may be.
 */
struct CBlockLocator
{
    /** Historically CBlockLocator's version field has been written to network
     * streams as the negotiated protocol version and to disk streams as the
     * client version, but the value has never been used.
     *
     * Hard-code to the highest protocol version ever written to a network stream.
     * SerParams can be used if the field requires any meaning in the future,
     **/
    static constexpr int DUMMY_VERSION = 70016;

    std::vector<uint256> vHave;

    CBlockLocator() {}

    explicit CBlockLocator(std::vector<uint256>&& have) : vHave(std::move(have)) {}

    SERIALIZE_METHODS(CBlockLocator, obj)
    {
        int nVersion = DUMMY_VERSION;
        READWRITE(nVersion);
        READWRITE(obj.vHave);
    }

    void SetNull()
    {
        vHave.clear();
    }

    bool IsNull() const
    {
        return vHave.empty();
    }
};

#endif // BITCOIN_PRIMITIVES_BLOCK_H
