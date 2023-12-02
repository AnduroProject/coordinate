// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <federation_deposit.h>
#include <federation_validator.h>
#include <node/blockstorage.h>
#include <rpc/util.h>
#include <rpc/client.h>
#include <univalue.h>
#include <core_io.h>


using node::BlockManager;
using node::ReadBlockFromDisk;

std::vector<FederationTxOut> tDeposits;
bool isValidationActivate = false;
std::string depositAddress = "";
std::string burnAddress = "";

void addDeposit(std::vector<FederationTxOut> txOuts) {
   if (txOuts.size() == 0) {
      return;
   }
   if(!isSignatureAlreadyExist(txOuts[0])) {
      for (const FederationTxOut& tx : txOuts) {
            tDeposits.push_back(tx);
            depositAddress = tx.depositAddress;
            burnAddress = tx.burnAddress;
      }
   }
}

bool isSignatureAlreadyExist(FederationTxOut txOut) {
   bool isExist = false;
   std::vector<FederationTxOut> pending_deposits = listPendingDepositTransaction(txOut.block_height);
   if(pending_deposits.size() > 0) {
      isExist = true;
   }
   return isExist;
}

bool isSpecialTxoutValid(std::vector<FederationTxOut> txOuts, ChainstateManager& chainman) {
 
   if(txOuts.size()==0) {
      return false;
   }
   LOCK(cs_main);
   CChain& active_chain = chainman.ActiveChain();
   int blockindex = active_chain.Height();
   if(txOuts[0].block_height <= blockindex) {
      blockindex = blockindex - 1;
   }

   CBlock block;
   if (!ReadBlockFromDisk(block, CHECK_NONFATAL(active_chain[blockindex]), Params().GetConsensus())) {
   }

   UniValue messages(UniValue::VARR);
   int tIndex = 1;
   for (const FederationTxOut& txOut : txOuts) {

      CTxDestination address;
      ExtractDestination(txOut.scriptPubKey, address);
      std::string addressStr = EncodeDestination(address);

      UniValue message(UniValue::VOBJ);
      message.pushKV("address", addressStr);
      message.pushKV("amount", txOut.nValue);
      message.pushKV("height", txOut.block_height);
      message.pushKV("index", tIndex);

      tIndex = tIndex + 1;
      messages.push_back(message);
   }

   LogPrintf("============== message is 1 %s \n",messages.write());

   bool isValid = validateFederationSignature(txOuts[0].witness,messages.write(),block.currentKeys);

   if (isValid) {
      LogPrintf("message is 2 \n");
      return true;
   }
   LogPrintf("message is 3 \n");
   return false;
}

std::vector<FederationTxOut> listPendingDepositTransaction(int32_t block_height) {
   if(block_height == -1) {
      return tDeposits;
   }
   std::vector<FederationTxOut> tDepositsNew;
    for (const FederationTxOut& tx_out : tDeposits) {
      if(tx_out.block_height == block_height) {
         tDepositsNew.push_back(tx_out);
      }
    }
   return tDepositsNew;
}

CAmount listPendingDepositTotal(int32_t block_height) {
   std::vector<FederationTxOut> tDepositsNew;
   if(block_height == -1) {
      tDepositsNew = tDeposits;
   } else {
    for (const FederationTxOut& tx_out : tDeposits) {
      if(tx_out.block_height == block_height) {
         tDepositsNew.push_back(tx_out);
      }
    }
   }


    CAmount totalDeposit = CAmount(0);
    for (const FederationTxOut& txOut: tDepositsNew) {
        totalDeposit = totalDeposit + CAmount(txOut.nValue);
    }
    return totalDeposit;
} 

void resetDeposit(int32_t block_height) {
   std::vector<FederationTxOut> tDepositsNew;
   bool hasDeposits = true;
   uint32_t currentHeight = block_height;
   while(hasDeposits) {
      for (const FederationTxOut& tx_out : tDeposits) {
         if(tx_out.block_height != block_height) {
            tDepositsNew.push_back(tx_out);
         }
      }
      currentHeight = currentHeight - 1;
      std::vector<FederationTxOut> pending_deposits = listPendingDepositTransaction(currentHeight);
      if(pending_deposits.size()==0) {
         hasDeposits = false;
      }
   }

   tDeposits = tDepositsNew;
}

std::string getCurrentKeys(ChainstateManager& chainman) {
   int block_height = chainman.ActiveChain().Height();
   LOCK(cs_main);
   CChain& active_chain = chainman.ActiveChain();
   CBlock block;
   if (!ReadBlockFromDisk(block, CHECK_NONFATAL(active_chain[block_height]), Params().GetConsensus())) {
   }
   return block.currentKeys;
}

int32_t getNextIndex(ChainstateManager& chainman) {
   int block_height = chainman.ActiveChain().Height();
   LOCK(cs_main);
   CChain& active_chain = chainman.ActiveChain();
   CBlock block;
   if (!ReadBlockFromDisk(block, CHECK_NONFATAL(active_chain[block_height]), Params().GetConsensus())) {
   }
   return block.nextIndex;
}

bool isFederationValidationActive() {
   return isValidationActivate;
}

bool verifyFederation(ChainstateManager& chainman, const CBlock& block) {
   return true;
   LOCK(cs_main);
   CChain& active_chain = chainman.ActiveChain();
   
   if(listPendingDepositTransaction(active_chain.Height()+1).size()>0) {
         isValidationActivate = true;
   }

   if(!isFederationValidationActive()) {
      return true;
   }


   LogPrintf("*********************** verifyCoinbase *********************** %s \n", block.vtx[0]->ToString());
   if(block.vtx[0]->vout.size() < 3) {
      return false;
   }
   LogPrintf("current block %i \n",active_chain.Height());
   CBlock prevblock;
   if (!ReadBlockFromDisk(prevblock, CHECK_NONFATAL(active_chain[active_chain.Height()]), Params().GetConsensus())) {
      return false;
   }

   LogPrintf("previous address %s \n",prevblock.currentKeys);
   LogPrintf("current address %s \n",block.currentKeys);

   std::vector<unsigned char> wData(ParseHex(prevblock.currentKeys));
   const std::string prevWitnessHexStr(wData.begin(), wData.end());
   UniValue witnessVal(UniValue::VOBJ);
   if (!witnessVal.read(prevWitnessHexStr)) {
      LogPrintf("invalid witness params \n");
      return false;
   }

   CTxOut witnessOut = block.vtx[0]->vout[block.vtx[0]->vout.size()-2];

   const std::string witnessStr = ScriptToAsmStr(witnessOut.scriptPubKey).replace(0,10,"");
   LogPrintf("testing federation validation 5 %s\n",witnessStr);

   std::vector<FederationTxOut> tOuts;
   if(block.vtx[0]->vout.size() == 3) {
      LogPrintf("testing federation validation 6 \n");
      const CTxDestination coinbaseScript = DecodeDestination(find_value(witnessVal.get_obj(), "current_address").get_str());
      const CScript scriptPubKey = GetScriptForDestination(coinbaseScript);
      FederationTxOut out(AmountFromValue(0), scriptPubKey, witnessStr, active_chain.Height() + 1,block.nextIndex,block.currentKeys, "", "");
      tOuts.push_back(out);
      LogPrintf("testing federation validation 7 \n");
   } else {
      for (size_t i = 1; i < block.vtx[0]->vout.size()-2; i=i+1) {  
         CTxOut pegTx = block.vtx[0]->vout[i];
         FederationTxOut out(pegTx.nValue, pegTx.scriptPubKey, witnessStr, active_chain.Height() + 1,block.nextIndex,block.currentKeys, "", "");
         tOuts.push_back(out);
      }
   } 
      
   if(!isSpecialTxoutValid(tOuts,chainman)) {
      return false;
   }

   return true;
}

std::string getDepositAddress() {
   return depositAddress;
}

std::string getBurnAddress() {
   return burnAddress;
}
