// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <federation_deposit.h>
#include <node/blockstorage.h>
#include <rpc/client.h>
#include <univalue.h>
#include <core_io.h>

using node::BlockManager;
using node::ReadBlockFromDisk;

std::vector<FederationTxOut> tDeposits;
std::string pegInfo;
std::string pegWitness;

void addDeposit(std::vector<FederationTxOut> txOuts) {
   LogPrintf("it is came to deposit");
   for (const FederationTxOut& tx : txOuts) {
      tDeposits.push_back(tx);
   }
}

bool isSignatureAlreadyExist(std::vector<FederationTxOut> txOuts) {
    bool isExist = true;
    for (const FederationTxOut& txOut : txOuts) {
       std::vector<FederationTxOut> pending_deposits = listPendingDepositTransaction(txOut.block_height);
       if(pending_deposits.size()>0) {
         if(pending_deposits.size()==1 && pending_deposits[0].nValue == 0) {
            resetDeposit(txOut.block_height);
         } else {
            isExist = false;
            break;
         }
       }
    }
   return isExist;
}

bool isSpecialTxoutValid(std::vector<FederationTxOut> txOuts, ChainstateManager& chainman) {
   
   if(txOuts.size()==0) {
      return false;
   }
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
      UniValue message(UniValue::VOBJ);
      message.pushKV("pegtime", txOut.pegTime);
      message.pushKV("height", txOut.block_height);
      message.pushKV("amount", txOut.nValue);
      message.pushKV("index", tIndex);
      message.pushKV("address", EncodeDestination(address));
      tIndex = tIndex + 1;
      messages.push_back(message);
   }

   UniValue mainstr(UniValue::VOBJ);
   mainstr.pushKV("message", messages);
   mainstr.pushKV("chain_id", "1");
   mainstr.pushKV("federationaddress", block.nextAddress);
   mainstr.pushKV("witness", txOuts[0].witness);
   mainstr.pushKV("network", getNetworkText(chainman));

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

std::string getNetworkText(ChainstateManager& chainman) {
   std::string networkIDString = chainman.GetParams().NetworkIDString();
   std::string networkText = "testnet";
   if (networkIDString.compare("regtest") == 0) {
      networkText = "regtest";
   } else if(networkIDString.compare("main") == 0) {
      networkText = "mainnet";
   }
   return networkText;
}

bool isPegInfoValid(std::string pegInfoIn, std::string pegWitness, ChainstateManager& chainman) {
   int block_height = chainman.ActiveChain().Height();
   CChain& active_chain = chainman.ActiveChain();
   int blockindex = chainman.ActiveChain().Height();


   CBlock block;
   if (!ReadBlockFromDisk(block, CHECK_NONFATAL(active_chain[blockindex]), Params().GetConsensus())) {
   }

   UniValue messages(UniValue::VARR);
   UniValue message(UniValue::VOBJ);
   message.pushKV("pegtime", 0);
   message.pushKV("height", 0);
   message.pushKV("amount", 0);
   message.pushKV("index", 0);
   message.pushKV("address", pegInfoIn);
   messages.push_back(message);

   UniValue mainstr(UniValue::VOBJ);
   mainstr.pushKV("message", messages);
   mainstr.pushKV("chain_id", "1");
   mainstr.pushKV("network", getNetworkText(chainman));
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


void addFederationPegout(std::string pegInfoIn, std::string pegWitnessIn) {
    pegWitness = pegWitnessIn;
    pegInfo = pegInfoIn;
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
   for (const FederationTxOut& tx_out : tDeposits) {
      if(tx_out.block_height != block_height) {
         tDepositsNew.push_back(tx_out);
      }
   }
   tDeposits = tDepositsNew;
}

void resetPegInfo(std::string pegInfoIn) {
   if (pegInfo.compare(pegInfoIn) == 0) {
         pegInfo = "";
         pegWitness = "";
   }
}

std::string getNextAddress(ChainstateManager& chainman) {
   int block_height = chainman.ActiveChain().Height();
   CChain& active_chain = chainman.ActiveChain();
   CBlock block;
   if (!ReadBlockFromDisk(block, CHECK_NONFATAL(active_chain[block_height]), Params().GetConsensus())) {
   }
   return block.nextAddress;
}

int32_t getNextIndex(ChainstateManager& chainman) {
   int block_height = chainman.ActiveChain().Height();
   CChain& active_chain = chainman.ActiveChain();
   CBlock block;
   if (!ReadBlockFromDisk(block, CHECK_NONFATAL(active_chain[block_height]), Params().GetConsensus())) {
   }
   return block.nextIndex;
}


std::string getPegInfo() {
   return pegInfo;
}

std::string getPegWitness() {
   return pegWitness;
}

std::string string_to_hex(const std::string& in) {
    std::stringstream ss;

    ss << std::hex << std::setfill('0');
    for (size_t i = 0; in.length() > i; ++i) {
        ss << std::setw(2) << static_cast<unsigned int>(static_cast<unsigned char>(in[i]));
    }

    return ss.str(); 
}

std::string hex_to_str(const std::string& in) {
   std::string s {in};
   return s;
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

bool verifyFederation(CChain& activeChain, const CBlock& block) {
   LogPrintf("*********************** verifyCoinbase *********************** %s \n", block.vtx[0]->ToString());
   // if(block.vtx[0]->vout.size() < 3 && activeChain.Height() > 1) {
   //    return false;
   // }
   std::vector<FederationTxOut> pending_deposits = listPendingDepositTransaction(activeChain.Height()+1);
   if(pending_deposits.size()==0) {
      return false;
   }
   // LogPrintf("current block %i \n",activeChain.Height());
   // CBlock prevblock;
   // if (!ReadBlockFromDisk(prevblock, CHECK_NONFATAL(activeChain[activeChain.Height()]), Params().GetConsensus())) {
   //    return false;
   // }

   // LogPrintf("previous address %s \n",prevblock.nextAddress);
   // LogPrintf("current address %s \n",block.nextAddress);
   // std::vector<FederationTxOut> tOuts;
   // if(block.vtx[0]->vout.size() == 3) {
   //    CTxOut witnessOut = block.vtx[0]->vout[block.vtx[0]->vout.size()-2];
   //    std::vector<unsigned char> txData(ParseHex(ScriptToAsmStr(witnessOut.scriptPubKey).replace(0,10,"")));
   //    const std::string witnessStr(txData.begin(), txData.end());
   //    UniValue val(UniValue::VOBJ);
   //    if (!val.read(witnessStr)) {
   //       return false
   //    }
   //    const CTxDestination coinbaseScript = DecodeDestination(prevblock.nextAddress);
   //    const CScript scriptPubKey = GetScriptForDestination(coinbaseScript);
   //    FederationTxOut out(AmountFromValue(0), scriptPubKey, find_value(val.get_obj(), "witness").get_str(), "0000000000000000000000000000000000000000000000000000000000000000", activeChain.Height() + 1,block.nextIndex,block.pegTime,block.nextAddress);
   //    tOuts.push_back(out);
   // }

   // if(!isSpecialTxoutValid(tOuts,activeChain)) {
   //    return false;
   // }

   return true;
}