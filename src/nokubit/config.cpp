// TODO NOKUBIT: header



#include <univalue.h>
#include <primitives/block.h>
#include <nokubit/config.h>
#include <nokubit/tagblock.h>
#include <policy/feerate.h>
#include <txmempool.h>
#include <streams.h>
#include <versionbits.h>


CFeeRate GetMinFee(const CTxMemPool& pool, const CTransaction& tx, size_t sizelimit)
{
    return pool.GetMinFee(sizelimit);
}

bool getsharingfee(const CAmount& txfee, const asset_tag::CAmountMap value_out, CAmount& txsharingfee)
{
    txsharingfee = txfee;
    if (!MoneyRangeAssetZero(value_out)) {
        txsharingfee *= 2;
        txsharingfee /= 5;
        return true;
    }
    return false;
}


void getsuperblockversion(const CBlockIndex* pindexPrev, int32_t& nVersion)
{
    if (block_tag::BLOCK_REWARD_PERIOD > 0 && pindexPrev != nullptr && pindexPrev->nHeight > 0) {
        if ((pindexPrev->nHeight + 1) % block_tag::BLOCK_REWARD_PERIOD == 0) {
            nVersion |= TAGBLOCK_BITS;
        // TODO valutare se blocco unico o spezzettato
        //} else if ((pindexPrev->nVersion & TAGBLOCK_BITS) != 0) {
        //    ...
        }
    }
}

void getsuperblockcaps(CBlock* pblock, UniValue& aCaps)
{
    if (pblock->nVersion & TAGBLOCK_BITS) {
        aCaps.push_back("feesharing");
    }
}

void getsuperblocktemplate(CBlock* pblock, UniValue& result)
{
    // TODO NOKUBIT: PAY_FEE_DA_VALUTARE
    if (pblock->nVersion & TAGBLOCK_BITS) {
        UniValue coinfee(UniValue::VARR);
        int i = 0;
        for (const auto& it : pblock->vtx[0]->vout) {
            const CTxOut& txout = it;
            if (i == 0)
                continue;

            CDataStream ssTxOut(SER_NETWORK, PROTOCOL_VERSION);
            ssTxOut << txout;
            coinfee.push_back(HexStr(ssTxOut.begin(), ssTxOut.end()));
        }
        result.pushKV("feesharing", coinfee);
    }
}

