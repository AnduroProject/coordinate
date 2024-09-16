#include <coordinate/coordinate_pegin.h>
#include <coordinate/coordinate_address.h>
#include <primitives/bitcoin/transaction.h>
#include <primitives/bitcoin/merkleblock.h>
#include <key_io.h>
#include <logging.h>
#include <util/strencodings.h>
#include <univalue.h>

CTxOut getPeginAmount(const std::vector<unsigned char>& bitcoinTx, const std::vector<unsigned char>& bitcoinTxProof, std::string depositAddress) {
    Sidechain::Bitcoin::CTransactionRef pegtx;
    CDataStream pegtx_stream(bitcoinTx, SER_NETWORK, PROTOCOL_VERSION);
    pegtx_stream >> pegtx;
    bool isOutputAvailable = false;
    CAmount value = 0;
    for (size_t i = 0; i < pegtx->vout.size(); i++) {
      CTxDestination fedAddress;
      ExtractDestination(pegtx->vout[i].scriptPubKey, fedAddress);
      std::string fedAddressStr = ParentEncodeDestination(fedAddress);
      if(depositAddress.compare(fedAddressStr) == 0) {
        isOutputAvailable = true;
        value = pegtx->vout[i].nValue;
        break;
      }
    }

    if(!isOutputAvailable || value == 0) {
      return CTxOut();
    }
    std::string userAddressStr = "";
    for (size_t i = 0; i < pegtx->vout.size(); i++) {
        if(pegtx->vout[i].scriptPubKey.IsUnspendable()) {
            std::vector<unsigned char> wData(ParseHex(ExtractOpReturnScript(pegtx->vout[i].scriptPubKey)));
            std::string finalStr(wData.begin(), wData.end());
            if (IsValidDestination(DecodeDestination(finalStr))) {
               userAddressStr = finalStr;
               break;
            } 
        }
    }
    if(userAddressStr.compare("") == 0) {
      LogPrintf("coordinate address is missing \n");
      return CTxOut();
    }
    const CTxDestination coinbaseScript = DecodeDestination(userAddressStr);
    return CTxOut(value, GetScriptForDestination(coinbaseScript));
}

bool hasAddressInRegistry(Chainstate& m_active_chainstate, std::string depositAddress) {
    CoordinateAddress coordinateAddress;
    m_active_chainstate.psignedblocktree->getDepositAddress(depositAddress,coordinateAddress);
    if(coordinateAddress.currentIndex == -1) {
        return false;
    }
    return true;
}

std::string ExtractOpReturnScript(const CScript& script) {
    std::string str;
    opcodetype opcode;
    std::vector<unsigned char> vch;
    CScript::const_iterator pc = script.begin();
    while (pc < script.end()) {
        if (!script.GetOp(pc, opcode, vch)) {
            str = "";
            return str;
        }
        if (0 <= opcode && opcode <= OP_PUSHDATA4) {
            if (vch.size() <= static_cast<std::vector<unsigned char>::size_type>(4)) {
                str += strprintf("%d", CScriptNum(vch, false).getint());
            } else {
              str += HexStr(vch);
            }
        } 
    }
    return str;
}

CTxIn buildPeginTxInput(const std::vector<unsigned char>& bitcoinTx, const std::vector<unsigned char>& bitcoinTxProof, std::string depositAddress, std::string depositAddress, CTxOut txOut) {
    Sidechain::Bitcoin::CTransactionRef pegtx;
    CDataStream pegtx_stream(bitcoinTx, SER_NETWORK, PROTOCOL_VERSION);
    pegtx_stream >> pegtx;

    uint32_t index = 0;

    for (size_t i = 0; i < pegtx->vout.size(); i++) {
      CTxDestination fedAddress;
      ExtractDestination(pegtx->vout[i].scriptPubKey, fedAddress);
      std::string fedAddressStr = ParentEncodeDestination(fedAddress);
      if(depositAddress.compare(fedAddressStr) == 0) {
        index = i;
        break;
      }
    }

    COutPoint prevOutput = COutPoint(pegtx->GetHash(), index);
}