

#ifndef BITCOIN_COORDINATEPEGIN_H
#define BITCOIN_COORDINATEPEGIN_H

#include <consensus/amount.h>
#include <primitives/transaction.h>
#include <validation.h>
#include <string>
#include <uint256.h>

static const char MAINCHAIN_RPC_HOST[] = "127.0.0.1";
static const int MAINCHAIN_RPC_PORT = 19011;
static const char MAINCHAIN_RPC_USER[] = "bitcoin";
static const char MAINCHAIN_RPC_PASS[] = "bitcoin";
static const int MAINCHAIN_HTTP_CLIENT_TIMEOUT=900;

class UniValue;

static const CAmount PEGIN_FEE=10;

UniValue CallMainChainRPC(const std::string& strMethod, const UniValue& params);

CTxOut getPeginAmount(const std::vector<unsigned char>& bitcoinTx, const std::vector<unsigned char>& bitcoinTxProof, std::string depositAddress);

bool hasAddressInRegistry(Chainstate& m_active_chainstate, std::string depositAddress);

std::string ExtractOpReturnScript(const CScript& script);

CTxIn buildPeginTxInput(const std::vector<unsigned char>& bitcoinTx, const std::vector<unsigned char>& bitcoinTxProof, std::string depositAddress, CTxOut txOut);

void saveAddressInRegistry(Chainstate& m_active_chainstate, std::string depositAddress);

/** Checks pegin witness for validity */
bool IsValidPeginWitness(const CScriptWitness& pegin_witness, const COutPoint& prevout, std::string& err_msg);

// Verify if the block with given hash has at least the specified minimum number
// of confirmations.
// For validating merkle blocks, you can provide the nbTxs parameter to verify if
// it equals the number of transactions in the block.
bool IsConfirmedBitcoinBlock(const uint256& hash, const int nMinConfirmationDepth, const int nbTxs);

bool isPeginFeeValid(const CTransaction& tx);

#endif // BITCOIN_COORDINATEPEGIN_H