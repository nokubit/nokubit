// TODO NOKUBIT: header


#ifndef BITCOIN_NOKUBIT_TAGBLOCK_H
#define BITCOIN_NOKUBIT_TAGBLOCK_H

//#include <amount.h>
//#include <script/script.h>
#include <script/standard.h>
//#include <uint256.h>
//#include <hash.h>
#include <nokubit/asset.h>
#include <consensus/validation.h>
//#include <primitives/transaction.h>
//#include <clientversion.h>
//#include <vector>
#include <stdlib.h>
#include <stdint.h>
//#include <string>


namespace block_tag {

static const int BLOCK_REWARD_PERIOD = CONF_BLOCK_REWARD_PERIOD;


/**
 * Information about a fee-sharing transaction.
 */
struct FeeSharingInfo
{
    /** The transaction itself */
    uint256 txHash;

    /** Block height of the transaction. */
    int nHeight;

    /** Fee destination address. */
    WitnessV0KeyHash feeAddress;

    /** The fee value. */
    int64_t nFee;
};


bool DumpFeeSharing(std::vector<FeeSharingInfo>& vinfo);

bool CheckTagBlockTransaction(const CTransaction& tx, CValidationState &state, bool fCheckDuplicateInputs);

}   // block_tag


#endif //  BITCOIN_NOKUBIT_TAGBLOCK_H
