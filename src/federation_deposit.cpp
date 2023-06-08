// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <federation_deposit.h>
#include <node/blockstorage.h>
#include <univalue.h>

using node::BlockManager;
using node::ReadBlockFromDisk;

std::vector<CTxOut> tDeposits;
std::string pegInfo;
std::string pegWitness;
std::string nextAddress;
int32_t pegTime = 0;
int32_t nextIndex = 0;

static const std::string GENSIS_NEXTADDRESS = "bcrt1pnqu5a0q80rjvjtavsal0mqgdwlgq3epl978jlmlquc2lgtfsxsgs5pvzys";

void addDeposit(const CTxOut& tx) {
    LogPrintf("it is came to deposit");
    tDeposits.push_back(tx);
}

bool isSpecialTxoutValid(const CTxOut& txOut, int32_t tIndex, int32_t pegTimeIn, ChainstateManager& chainman, bool isNew) {
   int block_height = chainman.ActiveChain().Height();
   CChain& active_chain = chainman.ActiveChain();
   int blockindex = chainman.ActiveChain().Height();
   if(isNew) {
      block_height = block_height + 1;
   }  else {
      blockindex = blockindex - 1;
   }

   CBlock block;
   if (!ReadBlockFromDisk(block, CHECK_NONFATAL(active_chain[blockindex]), Params().GetConsensus())) {
   }


   CTxDestination address;
   ExtractDestination(txOut.scriptPubKey, address);
   UniValue message(UniValue::VOBJ);
   message.pushKV("pegtime", pegTimeIn);
   message.pushKV("height", block_height);
   message.pushKV("amount", txOut.nValue);
   message.pushKV("index", tIndex);
   message.pushKV("address", EncodeDestination(address));

   UniValue mainstr(UniValue::VOBJ);
   mainstr.pushKV("message", message);
   mainstr.pushKV("federationaddress", block.nextAddress);
   mainstr.pushKV("witness", txOut.witness);

   std::string messagestr = "federation -sv '"  + mainstr.write() + "'";
   LogPrintf("******************messagestr********************* %s \n",messagestr);
   char* params = strcpy(new char[messagestr.length() + 1], messagestr.c_str());

   std::string res = exec(params);
   std::string expected = "success";
   res.erase(res.find_last_not_of(" \n\r\t")+1);

   LogPrintf("******************message response********************* %s \n",res);

   if (res.compare(expected) == 0) {
      return true;
   }

   LogPrintf("failed to check condition");

   return false;
}

bool isPegInfoValid(std::string pegInfoIn, std::string pegWitness, ChainstateManager& chainman, bool isNew) {
   int block_height = chainman.ActiveChain().Height();
   CChain& active_chain = chainman.ActiveChain();
   int blockindex = chainman.ActiveChain().Height();
   if(isNew) {
      block_height = block_height + 1;
   }  else {
      blockindex = blockindex - 1;
   }

   CBlock block;
   if (!ReadBlockFromDisk(block, CHECK_NONFATAL(active_chain[blockindex]), Params().GetConsensus())) {
   }


   UniValue message(UniValue::VOBJ);
   message.pushKV("pegtime", 0);
   message.pushKV("height", block_height);
   message.pushKV("amount", 0);
   message.pushKV("index", 0);
   message.pushKV("address", pegInfoIn);

   UniValue mainstr(UniValue::VOBJ);
   mainstr.pushKV("message", message);
   mainstr.pushKV("federationaddress", block.nextAddress);
   mainstr.pushKV("witness", pegWitness);

   std::string messagestr = "federation -sv '"  + mainstr.write() + "'";
   LogPrintf("******************messagestr********************* %s \n",messagestr);
   char* params = strcpy(new char[messagestr.length() + 1], messagestr.c_str());

   std::string res = exec(params);
   std::string expected = "success";
   res.erase(res.find_last_not_of(" \n\r\t")+1);

   LogPrintf("******************message response********************* %s \n",res);

   if (res.compare(expected) == 0) {
      return true;
   }

   LogPrintf("failed to check condition");

   return false;
}



void addFederationTransactionInfo(int32_t nextIndexIn, int32_t pegTimeIn, std::string nextAddressIn) {
    pegTime = pegTimeIn;
    nextAddress = nextAddressIn;
    nextIndex = nextIndexIn;
}

void addFederationPegout(std::string pegInfoIn, std::string pegWitnessIn) {
    pegWitness = pegWitnessIn;
    pegInfo = pegInfoIn;
}


std::vector<CTxOut> listPendingDepositTransaction() {
   return tDeposits;
}

CAmount listPendingDepositTotal() {
    CAmount totalDeposit = CAmount(0);
    for (const CTxOut& txOut: tDeposits) {
        totalDeposit = totalDeposit + CAmount(txOut.nValue);
    }
    return totalDeposit;
} 

void resetDeposit() {
   pegInfo = "";
   pegWitness = "";
   pegTime = 0;
   nextIndex = -1;
   tDeposits.clear();
}

std::string getNextAddress(ChainstateManager& chainman) {
   if(nextIndex == -1) {
      int block_height = chainman.ActiveChain().Height();
      CChain& active_chain = chainman.ActiveChain();
      CBlock block;
      if (!ReadBlockFromDisk(block, CHECK_NONFATAL(active_chain[block_height]), Params().GetConsensus())) {
      }
      return block.nextAddress;
   } else if (nextIndex == 0) {
      return GENSIS_NEXTADDRESS;
   }
   return nextAddress;
}

int32_t getNextIndex(ChainstateManager& chainman) {
   if(nextIndex == -1) {
      int block_height = chainman.ActiveChain().Height();
      CChain& active_chain = chainman.ActiveChain();
      CBlock block;
      if (!ReadBlockFromDisk(block, CHECK_NONFATAL(active_chain[block_height]), Params().GetConsensus())) {
      }
      return block.nextIndex;
   }
   return nextIndex;
}


std::string getPegInfo() {
   return pegInfo;
}

std::string getPegWitness() {
   return pegWitness;
}

int32_t getPegTime() {
   return pegTime;
}

std::string exec(const char* cmd)
{
    std::array<char, 128> buffer;
    std::string result;
    auto pipe = popen(cmd, "r");
    
    if (!pipe) throw std::runtime_error("popen() failed!");
    
    while (!feof(pipe))
    {
        if (fgets(buffer.data(), 128, pipe) != nullptr)
            result += buffer.data();
    }
    
    auto rc = pclose(pipe);
    
    if (rc == EXIT_SUCCESS)
    {
        std::cout << "SUCCESS\n";
    }
    else
    {
        std::cout << "FAILED\n";
    }
    
    return result;
}