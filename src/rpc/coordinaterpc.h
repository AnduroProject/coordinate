// Copyright (c) 2017-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPC_COORDINATERPC_H
#define BITCOIN_RPC_COORDINATERPC_H

#include <string>
#include <uint256.h>

static const char MAINCHAIN_RPC_HOST[] = "127.0.0.1";
static const int MAINCHAIN_RPC_PORT = 19011;
static const char MAINCHAIN_RPC_USER[] = "bitcoin";
static const char MAINCHAIN_RPC_PASS[] = "bitcoin";
static const int MAINCHAIN_HTTP_CLIENT_TIMEOUT=900;

class UniValue;

class CConnectionFailed : public std::runtime_error
{
public:

    explicit inline CConnectionFailed(const std::string& msg) :
        std::runtime_error(msg)
    {}

};

UniValue CallMainChainRPC(const std::string& strMethod, const UniValue& params);

// Verify if the block with given hash has at least the specified minimum number
// of confirmations.
// For validating merkle blocks, you can provide the nbTxs parameter to verify if
// it equals the number of transactions in the block.
bool IsConfirmedBitcoinBlock(const uint256& hash, const int nMinConfirmationDepth, const int nbTxs);


#endif // BITCOIN_RPC_COORDINATERPC_H
