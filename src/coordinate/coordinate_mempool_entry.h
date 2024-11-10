#include <iostream>
#include <uint256.h>
#include <serialize.h>
#include <consensus/amount.h>
#include <primitives/transaction.h>
#include <validation.h>


/**
 * Get asset total amount
 * @param[in] tx  transaction information to find what is the amount asset included
 * @param[in] m_active_chainstate  active chain state information
 * @param[in] amountAssetIn  returns total asset amount used in transaction
 * @param[in] currentAssetID  returns current asset id
 */
bool getAssetWithAmount(const CTransaction& tx, Chainstate& m_active_chainstate, CAmount& amountAssetIn, uint32_t& currentAssetID);

/**
 * Get asset ouput information for particular transaction
 * @param[in] tx  transaction, used to find asset ouputs
 * @param[in] m_active_chainstate  active chain state information
 */
int getAssetOutputCount(const CTransaction& tx, Chainstate& m_active_chainstate);

