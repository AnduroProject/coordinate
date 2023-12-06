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

/**
 *  This is call is used to receive or prepare federation presigned signature data along with federation deposit and withdrawal address
 */
class FederationTxOut
{
public:
    CAmount nValue;  /*!< amount going to process as pegin in sidechain */
    CScript scriptPubKey; /*!< script pubkey to receive the pegin amount in sidechain */
    std::string witness;  /*!< presigned signature information  */
    int32_t block_height; /*!< block height where the presigned signature belong to */
    std::string currentKeys; /*!< next key going to be included in block header */
    int32_t nextIndex; /*!< next derivation index going to be include in block header. This index is refer back by federation and identify the  current derivation path to be signed*/
    std::string depositAddress; /*!< hold current deposit address from bitcoin */
    std::string burnAddress; /*!< hold current withdrawal address from sidechain */

    FederationTxOut()
    {
        SetNull();
    }

    /**
     * @param[in] nValueIn          amount going to process as pegin in sidechain
     * @param[in] scriptPubKeyIn    script pubkey to receive the pegin amount in sidechain
     * @param[in] witnessIn         presigned signature information.
     * @param[in] block_heightIn    block height where the presigned signature belong to 
     * @param[in] nextIndexIn       next derivation index going to be include in block header.
     * @param[in] currentKeysIn     next key going to be included in block header.
     * @param[in] depositAddressIn  hold current deposit address from bitcoin.
     * @param[in] burnAddressIn     hold current withdrawal address from sidechain.
     */
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
        nValue = 0;
        scriptPubKey.clear();
        witness = "";
        block_height = 0;
        currentKeys = "";
        nextIndex = 0;
        depositAddress = "";
        burnAddress = "";
    }
};


/**
 * This function get recent bitcoin deposit address used to peg in
*/
std::string getDepositAddress();
/**
 * This function get sidechain withdrawal address used to peg out
*/
std::string getBurnAddress();
/**
 * This function include presigned signature from federation 
 * @param[in] txOuts  hold three block presigned signature.
*/
void includePreSignedSignature(std::vector<FederationTxOut> txOuts);
/**
 * This function check block are fully synced to start validating federation new presigned signature for upcoming blocks
*/
bool isFederationValidationActive();
/**
 * validate the federation signature on confirmed blocks
 * @param[in] chainman  used to find previous blocks based on active chain state to find federation current keys
 * @param[in] block  current block received from connectblock
*/
bool verifyFederation(ChainstateManager& chainman, const CBlock& block);
/**
 * This function check presigned signatures are valid
 * @param[in] txOuts  presigned signature for validation
 * @param[in] chainman  used to find next block number and active chain state
*/
bool isSpecialTxoutValid(std::vector<FederationTxOut> txOuts, ChainstateManager& chainman);
/**
 * This function list all presigned pegin details for upcoming blocks by height
 * @param[in] block_height  block height to find pending pegin
*/
std::vector<FederationTxOut> listPendingDepositTransaction(int32_t block_height);
/**
 * This function find total pegin amount for particular block
 * @param[in] block_height  block height to find pending pegin
*/
CAmount listPendingDepositTotal(int32_t block_height);
/**
 * This function used to check whether presigned signature already exist when received signature through peer message
 * @param[in] txOut  presigned signature details
*/
bool isSignatureAlreadyExist(FederationTxOut txOut);
/**
 * This function used to reset presigned signature for processed blocks
 * @param[in] block_height  block height to clear presigned signature
*/
void resetDeposit(int32_t block_height);
/**
 * This function used to get current keys to be signed for upcoming block
 * @param[in] chainman  used to get active chain state
*/
std::string getCurrentKeys(ChainstateManager& chainman);
/**
 * This function used to get current next index to be signed for upcoming block
 * @param[in] chainman  used to get active chain state
*/
int32_t getNextIndex(ChainstateManager& chainman);


#endif // BITCOIN_FEDERAITON_H
