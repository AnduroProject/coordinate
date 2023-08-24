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
    std::string pegInfo;
    std::string pegWitness;
    int32_t block_height;
    std::string nextAddress;
    int32_t pegTime;
    int32_t nextIndex;
    std::string depositAddress;
    std::string burnAddress;
    FederationTxOut()
    {
        SetNull();
    }

    FederationTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn, std::string witnessIn, std::string peg_hashIn, int32_t block_heightIn, int32_t nextIndexIn, int32_t pegTimeIn, std::string nextAddressIn, std::string pegInfoIn, std::string pegWitnessIn, std::string depositAddressIn, std::string burnAddressIn) {
        nValue = nValueIn;
        scriptPubKey = scriptPubKeyIn;
        witness = witnessIn;
        peg_hash = peg_hashIn;
        block_height = block_heightIn;
        pegTime = pegTimeIn;
        nextIndex = nextIndexIn;
        nextAddress = nextAddressIn;
        pegInfo = pegInfoIn;
        pegWitness = pegWitnessIn;
        depositAddress = depositAddressIn;
        burnAddress = burnAddressIn;
    }

    SERIALIZE_METHODS(FederationTxOut, obj) { 
        READWRITE(obj.nValue, obj.scriptPubKey, obj.peg_hash, obj.witness, obj.block_height, obj.nextAddress, obj.pegTime, obj.nextIndex, obj.pegInfo, obj.pegWitness, obj.depositAddress, obj.burnAddress); 
    }

    void SetNull()
    {
        nValue = -1;
        scriptPubKey.clear();
        peg_hash = "";
        pegInfo= "";
        pegWitness = "";
        witness = "";
        block_height = 0;
        nextAddress = "";
        pegTime = 0;
        nextIndex = 0;
        depositAddress = "";
        burnAddress = "";
    }
};

std::vector<FederationTxOut> tDeposits;
bool isValidationActivate = false;
std::string depositAddress = "";
std::string burnAddress = "";

std::string getDepositAddress();
std::string getBurnAddress();
std::string getNetworkText(ChainstateManager& chainman);
std::string exec(const char* cmd);
std::string string_to_hex(const std::string& in);
std::string hex_to_str(const std::string& in);
void addDeposit(std::vector<FederationTxOut> txOuts);
bool isFederationValidationActive();
bool verifyFederation(ChainstateManager& chainman, const CBlock& block);
bool isSpecialTxoutValid(std::vector<FederationTxOut> txOuts, ChainstateManager& chainman);
bool isPegInfoValid(std::string pegInfoIn, std::string pegWitness, ChainstateManager& chainman, int32_t block_height);
std::vector<FederationTxOut> listPendingDepositTransaction(int32_t block_height);
CAmount listPendingDepositTotal(int32_t block_height);
bool isSignatureAlreadyExist(FederationTxOut txOut);
void resetDeposit(int32_t block_height);
void addFederationPegout(std::string pegInfoIn, std::string pegWitnessIn, int32_t block_height);
std::string getNextAddress(ChainstateManager& chainman);
int32_t getNextIndex(ChainstateManager& chainman);


#endif // BITCOIN_FEDERAITON_H
