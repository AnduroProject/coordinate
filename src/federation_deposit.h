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
    std::string witness;
    int32_t block_height;
    std::string currentKeys;
    int32_t nextIndex;
    std::string depositAddress;
    std::string burnAddress;

    FederationTxOut()
    {
        SetNull();
    }

    FederationTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn, std::string witnessIn, int32_t block_heightIn, int32_t nextIndexIn, std::string currentKeysIn, std::string depositAddressIn, std::string burnAddressIn) {
        nValue = nValueIn;
        scriptPubKey = scriptPubKeyIn;
        witness = witnessIn;
        block_height = block_heightIn;
        nextIndex = nextIndexIn;
        currentKeys = currentKeysIn;
        depositAddress = depositAddressIn;
        burnAddress = burnAddressIn;
    }

    SERIALIZE_METHODS(FederationTxOut, obj) { 
        READWRITE(obj.nValue, obj.scriptPubKey, obj.witness, obj.block_height, obj.currentKeys, obj.nextIndex, obj.depositAddress, obj.burnAddress); 
    }

    void SetNull()
    {
        nValue = -1;
        scriptPubKey.clear();
        witness = "";
        block_height = 0;
        currentKeys = "";
        nextIndex = 0;
        depositAddress = "";
        burnAddress = "";
    }
};



std::string getDepositAddress();
std::string getBurnAddress();
void addDeposit(std::vector<FederationTxOut> txOuts);
bool isFederationValidationActive();
bool verifyFederation(ChainstateManager& chainman, const CBlock& block);
bool isSpecialTxoutValid(std::vector<FederationTxOut> txOuts, ChainstateManager& chainman);
std::vector<FederationTxOut> listPendingDepositTransaction(int32_t block_height);
CAmount listPendingDepositTotal(int32_t block_height);
bool isSignatureAlreadyExist(FederationTxOut txOut);
void resetDeposit(int32_t block_height);
std::string getCurrentKeys(ChainstateManager& chainman);
int32_t getNextIndex(ChainstateManager& chainman);


#endif // BITCOIN_FEDERAITON_H
