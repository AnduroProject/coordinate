

#ifndef BITCOIN_COORDINATEPEGIN_H
#define BITCOIN_COORDINATEPEGIN_H

#include <consensus/amount.h>
#include <primitives/transaction.h>
#include <validation.h>

CTxOut getPeginAmount(const std::vector<unsigned char>& bitcoinTx, const std::vector<unsigned char>& bitcoinTxProof, std::string depositAddress);

bool hasAddressInRegistry(Chainstate& m_active_chainstate, std::string depositAddress);

std::string ExtractOpReturnScript(const CScript& script);

CTxIn buildPeginTxInput(const std::vector<unsigned char>& bitcoinTx, const std::vector<unsigned char>& bitcoinTxProof, std::string depositAddress, CTxOut txOut);


#endif // BITCOIN_COORDINATEPEGIN_H