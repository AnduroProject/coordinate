
#include <rpc/blockchain.h>
#include <kernel/mempool_persist.h>
#include <chainparams.h>
#include <core_io.h>
#include <fs.h>
#include <kernel/mempool_entry.h>
#include <node/mempool_persist_args.h>
#include <policy/rbf.h>
#include <policy/settings.h>
#include <primitives/transaction.h>
#include <rpc/server.h>
#include <rpc/server_util.h>
#include <rpc/util.h>
#include <txmempool.h>
#include <univalue.h>
#include <util/moneystr.h>
#include <util/time.h>
#include <utility>
#include <anduro_deposit.h>
#include <anduro_validator.h>

using node::DEFAULT_MAX_RAW_TX_FEE_RATE;
using node::NodeContext;


static RPCHelpMan sendpreconftransaction()
{
    return RPCHelpMan{"sendpreconftransaction",
        "\nSubmit a raw transaction (serialized, hex-encoded) to local node and network.\n"
        "\nThe transaction will be sent unconditionally to all peers, so using sendpreconftransaction\n"
        "for manual rebroadcast may degrade privacy by leaking the transaction's origin, as\n"
        "nodes will normally not rebroadcast non-wallet transactions already in their mempool.\n"
        "\nA specific exception, RPC_TRANSACTION_ALREADY_IN_CHAIN, may throw if the transaction cannot be added to the mempool.\n"
        "\nRelated RPCs: createrawtransaction, signrawtransactionwithkey\n",
        {
            {"hexstring", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "The hex string of the raw transaction"},
            {"maxfeerate", RPCArg::Type::AMOUNT, RPCArg::Default{FormatMoney(DEFAULT_MAX_RAW_TX_FEE_RATE.GetFeePerK())},
             "Reject transactions whose fee rate is higher than the specified value, expressed in " + CURRENCY_UNIT +
                 "/kvB.\nSet to 0 to accept any fee rate.\n"},
            {"assethexstring", RPCArg::Type::STR_HEX, RPCArg::Optional::OMITTED, "The hex string of the asset data"},
        },
        RPCResult{
            RPCResult::Type::STR_HEX, "", "The transaction hash in hex"
        },
        RPCExamples{
            "\nCreate a transaction\n"
            + HelpExampleCli("createrawtransaction", "\"[{\\\"txid\\\" : \\\"mytxid\\\",\\\"vout\\\":0}]\" \"{\\\"myaddress\\\":0.01}\"") +
            "Sign the transaction, and get back the hex\n"
            + HelpExampleCli("signrawtransactionwithwallet", "\"myhex\"") +
            "\nSend the transaction (signed hex)\n"
            + HelpExampleCli("sendrawtransaction", "\"signedhex\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("sendrawtransaction", "\"signedhex\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {

            CMutableTransaction mtx;
            if (!DecodeHexTx(mtx, request.params[0].get_str())) {
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed. Make sure the tx has at least one input.");
            }
            CTransactionRef tx(MakeTransactionRef(std::move(mtx)));
            const CFeeRate max_raw_tx_fee_rate = request.params[1].isNull() ?
                                                    DEFAULT_MAX_RAW_TX_FEE_RATE :
                                                    CFeeRate(AmountFromValue(request.params[1]));

            int64_t virtual_size = GetVirtualTransactionSize(*tx);
            CAmount max_raw_tx_fee = max_raw_tx_fee_rate.GetFee(virtual_size);
            std::string err_string;
            AssertLockNotHeld(cs_main);
            NodeContext& node = EnsureAnyNodeContext(request.context);
            const CTransaction& ptx = *tx;  

            if(ptx.nVersion != TRANSACTION_PRECONF_VERSION) {
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "asset data missing to submit in mempool");
            }

            const TransactionError err = BroadcastTransaction(node, tx, err_string, max_raw_tx_fee, /*relay=*/true, /*wait_callback=*/true, true);
            if (TransactionError::OK != err) {
                throw JSONRPCTransactionError(err, err_string);
            }
                
            return tx->GetHash().GetHex();
            
        },
    };
}




void RegisterPreConfMempoolRPCCommands(CRPCTable& t)
{
    static const CRPCCommand commands[]{
        {"preconf", &sendpreconftransaction},
    };
    for (const auto& c : commands) {
        t.appendCommand(c.name, &c);
    }
}
