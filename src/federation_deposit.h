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



std::string exec(const char* cmd);
void addDeposit(const CTxOut& tx);
bool isSpecialTxoutValid(const CTxOut& txOut, int32_t tIndex, int32_t pegTimeIn, ChainstateManager& chainman, bool isNew);
bool isPegInfoValid(std::string pegInfoIn, std::string pegWitness, ChainstateManager& chainman, bool isNew);
std::vector<CTxOut> listPendingDepositTransaction();
CAmount listPendingDepositTotal();
void resetDeposit();
void addFederationTransactionInfo(int32_t nextTimeIn,int32_t pegTimeIn, std::string nextAddressIn);
void addFederationPegout(std::string pegInfoIn, std::string pegWitnessIn);
std::string getNextAddress(ChainstateManager& chainman);
std::string getPegInfo();
std::string getPegWitness();
int32_t getNextIndex(ChainstateManager& chainman);
int32_t getPegTime();


#endif // BITCOIN_FEDERAITON_H
