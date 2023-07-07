// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_FEDERAITON_H
#define BITCOIN_FEDERAITON_H

#include <iostream>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <validation.h>
#include <key_io.h>
#include <primitives/transaction.h>
#include <logging.h>
#include <util/message.h>

class FederationTxOut
{
public:
    CAmount nValue;
    CScript scriptPubKey;
    std::string peg_hash;
    std::string witness;
    int32_t block_height;
    std::string nextAddress;
    int32_t pegTime;
    int32_t nextIndex;
    FederationTxOut()
    {
        SetNull();
    }

    FederationTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn, std::string witnessIn, std::string peg_hashIn, int32_t block_heightIn, int32_t nextIndexIn, int32_t pegTimeIn, std::string nextAddressIn) {
        nValue = nValueIn;
        scriptPubKey = scriptPubKeyIn;
        witness = witnessIn;
        peg_hash = peg_hashIn;
        block_height = block_heightIn;
        pegTime = pegTimeIn;
        nextIndex = nextIndexIn;
        nextAddress = nextAddressIn;
    }

    SERIALIZE_METHODS(FederationTxOut, obj) { 
        READWRITE(obj.nValue, obj.scriptPubKey, obj.peg_hash, obj.witness, obj.block_height, obj.nextAddress, obj.pegTime, obj.nextIndex); 
    }

    void SetNull()
    {
        nValue = -1;
        scriptPubKey.clear();
        peg_hash = "";
        witness = "";
        block_height = 0;
        nextAddress = "";
        pegTime = 0;
        nextIndex = 0;
    }
};


std::string exec(const char* cmd);
std::string string_to_hex(const std::string& in);
void addDeposit(std::vector<FederationTxOut> txOuts);
bool isSpecialTxoutValid(std::vector<FederationTxOut> txOuts, ChainstateManager& chainman);
bool isPegInfoValid(std::string pegInfoIn, std::string pegWitness, ChainstateManager& chainman);
std::vector<FederationTxOut> listPendingDepositTransaction(int32_t block_height);
CAmount listPendingDepositTotal(int32_t block_height);
void resetDeposit(int32_t block_height);
void resetPegInfo(std::string pegInfoIn);
void addFederationPegout(std::string pegInfoIn, std::string pegWitnessIn);
std::string getPegInfo();
std::string getPegWitness();

std::string getNextAddress(ChainstateManager& chainman);
int32_t getNextIndex(ChainstateManager& chainman);


#endif // BITCOIN_FEDERAITON_H
