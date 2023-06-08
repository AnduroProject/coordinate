// Copyright (c) 2018-2022 Daniel Kraft
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/auxpow_miner.h>

#include <arith_uint256.h>
#include <auxpow.h>
#include <chainparams.h>
#include <consensus/merkle.h>
#include <net.h>
#include <node/context.h>
#include <rpc/blockchain.h>
#include <rpc/protocol.h>
#include <rpc/request.h>
#include <rpc/server_util.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <util/strencodings.h>
#include <util/time.h>
#include <auxpow.h>
#include <federation_deposit.h>
#include <cassert>

namespace
{

using node::BlockAssembler;

void auxMiningCheck(const node::NodeContext& node)
{
  const auto& connman = EnsureConnman (node);
  const auto& chainman = EnsureChainman (node);

  if (connman.GetNodeCount (ConnectionDirection::Both) == 0
        && !Params ().MineBlocksOnDemand ())
    throw JSONRPCError (RPC_CLIENT_NOT_CONNECTED,
                        "MaraChain is not connected!");

  if (chainman.ActiveChainstate ().IsInitialBlockDownload ()
        && !Params ().MineBlocksOnDemand ())
    throw JSONRPCError (RPC_CLIENT_IN_INITIAL_DOWNLOAD,
                        "MaraChain is downloading blocks...");

  /* This should never fail, since the chain is already
     past the point of merge-mining start.  Check nevertheless.  */
  {
    LOCK (cs_main);
    const auto auxpowStart = Params ().GetConsensus ().nAuxpowStartHeight;
    if (chainman.ActiveHeight () + 1 < auxpowStart)
      throw std::runtime_error ("mining auxblock method is not yet available");
  }
}

}  // anonymous namespace

const CBlock*
AuxpowMiner::getCurrentBlock (const ChainstateManager& chainman,
                              const CTxMemPool& mempool,
                              const CScript& scriptPubKey, uint256& target)
{
  AssertLockHeld (cs);
  const CBlock* pblockCur = nullptr;

  {
    LOCK (cs_main);
    CScriptID scriptID (scriptPubKey);
    auto iter = curBlocks.find(scriptID);
    if (iter != curBlocks.end())
      pblockCur = iter->second;

    if (pblockCur == nullptr
        || pindexPrev != chainman.ActiveChain ().Tip ()
        || (mempool.GetTransactionsUpdated () != txUpdatedLast
            && GetTime () - startTime > 60))
      {
        if (pindexPrev != chainman.ActiveChain ().Tip ())
          {
            /* Clear old blocks since they're obsolete now.  */
            blocks.clear ();
            templates.clear ();
            curBlocks.clear ();
          }

        /* Create new block with nonce = 0 and extraNonce = 1.  */
        std::unique_ptr<node::CBlockTemplate> newBlock
            = BlockAssembler (chainman.ActiveChainstate (), &mempool)
                .CreateNewBlock (scriptPubKey);
        if (newBlock == nullptr)
          throw JSONRPCError (RPC_OUT_OF_MEMORY, "out of memory");

        /* Update state only when CreateNewBlock succeeded.  */
        txUpdatedLast = mempool.GetTransactionsUpdated ();
        pindexPrev = chainman.ActiveTip ();
        startTime = GetTime ();

        /* Finalise it by setting the version and building the merkle root.  */
        newBlock->block.hashMerkleRoot = BlockMerkleRoot (newBlock->block);
        newBlock->block.SetAuxpowVersion (true);

        /* Save in our map of constructed blocks.  */
        pblockCur = &newBlock->block;
        curBlocks.emplace(scriptID, pblockCur);
        blocks[pblockCur->GetHash ()] = pblockCur;
        templates.push_back (std::move (newBlock));
      }
  }

  /* At this point, pblockCur is always initialised:  If we make it here
     without creating a new block above, it means that, in particular,
     pindexPrev == ::ChainActive ().Tip().  But for that to happen, we must
     already have created a pblockCur in a previous call, as pindexPrev is
     initialised only when pblockCur is.  */
  assert (pblockCur);

  arith_uint256 arithTarget;
  bool fNegative, fOverflow;
  arithTarget.SetCompact (pblockCur->nBits, &fNegative, &fOverflow);
  if (fNegative || fOverflow || arithTarget == 0)
    throw std::runtime_error ("invalid difficulty bits in block");
  target = ArithToUint256 (arithTarget);

  return pblockCur;
}

const CBlock*
AuxpowMiner::lookupSavedBlock (const std::string& hashHex) const
{
  AssertLockHeld (cs);

  uint256 hash;
  hash.SetHex (hashHex);

  const auto iter = blocks.find (hash);
  if (iter == blocks.end ())
    throw JSONRPCError (RPC_INVALID_PARAMETER, "block hash unknown");

  return iter->second;
}


UniValue
AuxpowMiner::createAuxBlock (const JSONRPCRequest& request,
                             const CScript& scriptPubKey)
{
  LOCK (cs);

  const auto& node = EnsureAnyNodeContext (request);
  // auxMiningCheck (node);
  const auto& mempool = EnsureMemPool (node);
  const auto& chainman = EnsureChainman (node);

  uint256 target;
  const CBlock* pblock = getCurrentBlock (chainman, mempool, scriptPubKey, target);
  UniValue result(UniValue::VOBJ);
  result.pushKV ("hash", pblock->GetHash ().GetHex ());
  result.pushKV ("chainid", pblock->GetChainId ());
  result.pushKV ("previousblockhash", pblock->hashPrevBlock.GetHex ());
  result.pushKV ("coinbasevalue",
                 static_cast<int64_t> (pblock->vtx[0]->vout[0].nValue));
  result.pushKV ("bits", strprintf ("%08x", pblock->nBits));
  result.pushKV ("height", static_cast<int64_t> (pindexPrev->nHeight + 1));
  result.pushKV ("_target", HexStr (target));

  return result;
}

bool
AuxpowMiner::submitAuxBlock (const JSONRPCRequest& request,
                             const std::string& hashHex,
                             const std::string& blockHash,
                             const std::string& hashPrevBlock,
                             const std::string& hashMerkleRoot,
                             const std::string& nVersion,
                             const std::string& nTime,
                             const std::string& nBits,
                             const std::string& nNonce,
                             const std::string& coinbase) const
{
  const auto& node = EnsureAnyNodeContext (request);
  // auxMiningCheck (node);
  auto& chainman = EnsureChainman (node);

  std::shared_ptr<CBlock> shared_block;
  {
    LOCK (cs);
    const CBlock* pblock = lookupSavedBlock (hashHex);
    shared_block = std::make_shared<CBlock> (*pblock);
  }

  /* Build a minimal coinbase script input for merge-mining.  */
  // const uint256 blockHash = header.GetHash ();
  uint256 hash(ParseHashV(blockHash, "blockhash"));
  uint256 prevblock(ParseHashV(hashPrevBlock, "blockhash"));
  uint256 merkle(ParseHashV(hashMerkleRoot, "blockhash"));
  /*valtype inputData(hash.begin (), hash.end ());
  std::reverse (inputData.begin (), inputData.end ());
  inputData.push_back (1);
  inputData.insert (inputData.end (), 7, 0);*/

  /* Fake a parent-block coinbase with just the required input
     script and no outputs.  */
  /*CMutableTransaction coinbase;
  coinbase.vin.resize (1);
  coinbase.vin[0].prevout.SetNull ();
  coinbase.vin[0].scriptSig = (CScript () << inputData);
  assert (coinbase.vout.empty ());
  CTransactionRef coinbaseRef = MakeTransactionRef (coinbase);*/
  
  /*CDataStream ss(ParseHex (coinbase), SER_DISK, CLIENT_VERSION);
  CTransaction tx(deserialize, ss);
  CTransactionRef coinbaseRef = MakeTransactionRef (tx);*/

  CMutableTransaction tx;
  DecodeHexTx(tx, coinbase);
  CTransactionRef coinbaseRef = MakeTransactionRef(std::move(tx));

  /* Build a fake parent block with the coinbase.  */
  unsigned long int version = strtoul(nVersion.c_str(), NULL, 10);
  unsigned long int time = strtoul(nTime.c_str(), NULL, 10);
  unsigned long int bits = strtoul(nBits.c_str(), NULL, 10);
  unsigned long int nonce = strtoul(nNonce.c_str(), NULL, 10);
  CBlock parent;
  parent.nVersion = (uint32_t)version;
  parent.nTime = (uint32_t)time;
  parent.nBits = (uint32_t)bits;
  parent.nNonce = (uint32_t)nonce;
  parent.vtx.resize (1);
  parent.vtx[0] = coinbaseRef;
  parent.hashMerkleRoot = merkle;
  parent.hashPrevBlock = prevblock;

  /* Construct the auxpow object.  */
  std::unique_ptr<CAuxPow> auxpow(new CAuxPow (std::move (coinbaseRef)));
  assert (auxpow->vMerkleBranch.empty ());
  assert (auxpow->vChainMerkleBranch.empty ());
  auxpow->nChainIndex = 0;
  auxpow->parentBlock = parent;

  // const std::vector<unsigned char> vchAuxPow = ParseHex (auxpowHex);
  // CDataStream ss(vchAuxPow, SER_GETHASH, PROTOCOL_VERSION);
  // std::unique_ptr<CAuxPow> auxpow(new CAuxPow ());
  // ss >> *auxpow;
  shared_block->SetAuxpow (std::move (auxpow));
  assert (shared_block->GetHash ().GetHex () == hashHex);

  return chainman.ProcessNewBlock (shared_block, /*force_processing=*/true,
                                   /*min_pow_checked=*/true, nullptr);
}

AuxpowMiner&
AuxpowMiner::get ()
{
  static AuxpowMiner* instance = nullptr;
  static RecursiveMutex lock;

  LOCK (lock);
  if (instance == nullptr)
    instance = new AuxpowMiner ();

  return *instance;
}
