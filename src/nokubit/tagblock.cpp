// TODO NOKUBIT: header


#include <nokubit/tagblock.h>
#include <nokubit/tagscript.h>
#include <nokubit/organization.h>
#include <nokubit/assetname.h>
#include <validation.h>
#include <pow.h>
#include <chainparams.h>
#include <version.h>
#include <streams.h>
#include <txdb.h>
//#include <hash.h>
//#include <pubkey.h>
//#include <univalue.h>
#include <undo.h>

// TODO NOKUBIT: per log
#include <core_io.h>
#include <util.h>
#include <utilstrencodings.h>
#include <utilmoneystr.h>


#define MICRO 0.000001
#define MILLI 0.001

namespace block_tag {

static const uint64_t FEE_SHARING_DUMP_VERSION = 1;

std::vector<FeeSharingInfo> feesharing();

// Copied from validation.cpp
/** Open an undo file (rev?????.dat) */
static FILE* OpenUndoFile(const CDiskBlockPos &pos) {
    if (pos.IsNull())
        return nullptr;
    fs::path path = GetBlockPosFilename(pos, "rev");
    FILE* file = fsbridge::fopen(path, "rb");
    if (!file) {
        LogPrintf("Unable to open file %s\n", path.string());
        return nullptr;
    }
    if (pos.nPos) {
        if (fseek(file, pos.nPos, SEEK_SET)) {
            LogPrintf("Unable to seek to position %u of %s\n", pos.nPos, path.string());
            fclose(file);
            return nullptr;
        }
    }
    return file;
}

// Copied from validation.cpp
static bool UndoReadFromDisk(CBlockUndo& blockundo, const CBlockIndex *pindex)
{
    CDiskBlockPos pos = pindex->GetUndoPos();
    if (pos.IsNull()) {
        return error("%s: no undo data available", __func__);
    }

    // Open history file to read
    CAutoFile filein(OpenUndoFile(pos), SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
        return error("%s: OpenUndoFile failed", __func__);

    // Read block
    uint256 hashChecksum;
    CHashVerifier<CAutoFile> verifier(&filein); // We need a CHashVerifier as reserializing may lose data
    try {
        verifier << pindex->pprev->GetBlockHash();
        verifier >> blockundo;
        filein >> hashChecksum;
    }
    catch (const std::exception& e) {
        return error("%s: Deserialize or I/O error - %s", __func__, e.what());
    }

    // Verify checksum
    if (hashChecksum != verifier.GetHash())
        return error("%s: Checksum mismatch", __func__);

    return true;
}

//bool LoadFeeSharing(std::vector<FeeSharingInfo>& vinfo)
//{
//    const CChainParams& chainparams = Params();
//    FILE* filestr = fsbridge::fopen(GetDataDir() / "fee-sharing.dat", "rb");
//    CAutoFile file(filestr, SER_DISK, CLIENT_VERSION);
//    if (file.IsNull()) {
//        LogPrintf("Failed to open fee-sharing file from disk. Continuing anyway.\n");
//        return false;
//    }
//
//    int64_t count = 0;
//    int64_t expired = 0;
//    int64_t failed = 0;
//    int64_t already_there = 0;
//    int64_t nNow = GetTime();
//
//    try {
//        uint64_t version;
//        file >> version;
//        if (version != FEE_SHARING_DUMP_VERSION) {
//            return false;
//        }
//        uint64_t num;
//        file >> num;
//        while (num--) {
//            CTransactionRef tx;
//            int64_t nTime;
//            int64_t nFeeDelta;
//            file >> tx;
//            file >> nTime;
//            file >> nFeeDelta;
//
//            CAmount amountdelta = nFeeDelta;
//            if (amountdelta) {
//                feesharing.PrioritiseTransaction(tx->GetHash(), amountdelta);
//            }
//            CValidationState state;
//            if (nTime + nExpiryTimeout > nNow) {
//                LOCK(cs_main);
//                AcceptToMemoryPoolWithTime(chainparams, feesharing, state, tx, nullptr /* pfMissingInputs */, nTime,
//                                           nullptr /* plTxnReplaced */, false /* bypass_limits */, 0 /* nAbsurdFee */);
//                if (state.IsValid()) {
//                    ++count;
//                } else {
//                    // fee-sharing may contain the transaction already, e.g. from
//                    // wallet(s) having loaded it while we were processing
//                    // fee-sharing transactions; consider these as valid, instead of
//                    // failed, but mark them as 'already there'
//                    if (feesharing.exists(tx->GetHash())) {
//                        ++already_there;
//                    } else {
//                        ++failed;
//                    }
//                }
//            } else {
//                ++expired;
//            }
//            if (ShutdownRequested())
//                return false;
//        }
//        std::map<uint256, CAmount> mapDeltas;
//        file >> mapDeltas;
//
//        for (const auto& i : mapDeltas) {
//            feesharing.PrioritiseTransaction(i.first, i.second);
//        }
//    } catch (const std::exception& e) {
//        LogPrintf("Failed to deserialize fee-sharing data on disk: %s. Continuing anyway.\n", e.what());
//        return false;
//    }
//
//    LogPrintf("Imported fee-sharing transactions from disk: %i succeeded, %i failed, %i expired, %i already there\n", count, failed, expired, already_there);
//    return true;
//}

struct AssetInfo {
    const std::string name;
    const WitnessV0KeyHash address;
    const WitnessV0KeyHash state;
    const COutPoint outpoint;
    const int height;
    AssetInfo(const std::string& name, const WitnessV0KeyHash& address, const WitnessV0KeyHash& state, const COutPoint& outpoint, int height)
        : name(name), address(address), state(state), outpoint(outpoint), height(height) {}
};

bool DumpFeeSharing(std::vector<FeeSharingInfo>& vinfo)
{
    int64_t start = GetTimeMicros();

    FILE* filestr = fsbridge::fopen(GetDataDir() / "fee-sharing.dat", "rb");
    if (filestr != nullptr) {
        ::fclose(filestr);
        LogPrintf("The file fee-sharing exist.\n");
        return true;
    }

    const CChainParams& chainparams = Params();

    asset_db::Organization organizations;
    asset_db::AssetName names;
    std::map<std::string, WitnessV0KeyHash> assets;
    std::map<std::string, WitnessV0KeyHash> assetOrganizations;
    CBlockIndex* pindex;
    std::vector<CBlockIndex*> blocks;
    blocks.reserve(BLOCK_REWARD_PERIOD - 2);

    {
//AssertLockHeld(cs_main);
        LOCK(cs_main);

        if (chainActive.Tip() != nullptr && chainActive.Height() > BLOCK_REWARD_PERIOD) {
            CCoinsViewCache coins(pcoinsdbview.get());

            CBlockIndex* pindexTip = chainActive.Tip();

            int last = (chainActive.Height() / BLOCK_REWARD_PERIOD) * BLOCK_REWARD_PERIOD;
            int first = last - BLOCK_REWARD_PERIOD;
            pindex = pindexTip->GetAncestor(last);
            if (pindex == nullptr || (pindex->nVersion & TAGBLOCK_BITS) == 0)
                return error("DumpFeeSharing: Last block not superblock %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
            for (pindex = pindex->pprev; pindex && pindex->nHeight > first; pindex = pindex->pprev) {
                blocks.push_back(pindex);
            } 
            if (pindex == nullptr || pindex->nHeight != first)
                return error("DumpFeeSharing: Error on block %d", (pindex == nullptr ? -1 : pindex->nHeight));
            if ((pindex->nVersion & TAGBLOCK_BITS) == 0 && first != 0)
                return error("DumpFeeSharing: First block not superblock %d", pindex->nHeight);
        }
    }

    if (blocks.size() > 0) {
        CCoinsViewCache coins(pcoinsdbview.get());
        int nGoodTransactions = 0;
        CValidationState state;
        int reportDone = 0;

        for (std::vector<CBlockIndex*>::reverse_iterator rit = blocks.rbegin(); rit != blocks.rend(); ++rit) {
            CBlock block;
            pindex = *rit;
            if (!ReadBlockFromDisk(block, pindex, chainparams.GetConsensus()))
                return error("DumpFeeSharing: *** ReadBlockFromDisk failed at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
            if (!CheckBlock(block, state, chainparams.GetConsensus()))
                return error("DumpFeeSharing: *** found bad block at %d, hash=%s (%s)\n", pindex->nHeight, pindex->GetBlockHash().ToString(), FormatStateMessage(state));

            CBlockUndo blockUndo;
            if (!UndoReadFromDisk(blockUndo, pindex)) {
                return error("DumpFeeSharing(): failure reading undo data");
            }

            if (blockUndo.vtxundo.size() + 1 != block.vtx.size()) {
                return error("DumpFeeSharing(): block and undo data inconsistent");
            }

            for (unsigned int j = 1; j < block.vtx.size(); j++) {
                const CTransaction &tx = *(block.vtx[j]);
// TODO NOKUBIT: log
LogPrint(BCLog::ASSET, "\tParse: %s\n", tx.ToString());

                // not coinbases
                CTxUndo& txundo = blockUndo.vtxundo[j-1];
                // Tag overwritten
                const Coin& undoCoin = txundo.vprevout[0];
                if (undoCoin.fCoinType == CoinTypeTag) {
                    txundo.vprevout.erase(txundo.vprevout.begin());
                }
                if (txundo.vprevout.size() != tx.vin.size()) {
                    return error("DisconnectBlock(): transaction and undo data inconsistent");
                }

                asset_tag::CAmountMap nValueIn;
                for (unsigned int k = 0; k < tx.vin.size(); ++k) {
                    const CTxIn& txin = tx.vin[k];

                    if (k == 0 && txin.IsTagged()) {
                        if (!txin.IsPegin()) {
                            const std::vector<unsigned char>& vch = txin.scriptWitness.stack[0];
                            const asset_tag::NkbTagIdentifier pegType = (asset_tag::NkbTagIdentifier)vch[3];
                            const asset_tag::ScriptTag* ptag = asset_tag::ScriptTag::ExtractTag(pegType, txin.scriptWitness);
                            if (!ptag->IsValidate())
                                continue;

                            if (pegType == asset_tag::NkbTagIdentifier::NkbOrganization) {
                                const asset_tag::OrganizationScript* p = static_cast<const asset_tag::OrganizationScript*>(ptag);
                                organizations.Add(*p, pindex->nHeight);
                            } else if (pegType == asset_tag::NkbTagIdentifier::NkbMintAsset) {
                                const asset_tag::MintAssetScript* p = static_cast<const asset_tag::MintAssetScript*>(ptag);
                                names.Add(*p, pindex->nHeight);
                            }

                        // if (pegType == asset_tag::NkbTagIdentifier::NkbOrganization)
                        //     continue;
                        }
                    }

                    const COutPoint &prevout = txin.prevout;
                    const Coin& coin = txundo.vprevout[k];
                    assert(!coin.IsSpent());

                    // Check for negative or overflow input values
                    nValueIn += coin.out.nValue;
                    if (!MoneyRange(coin.out.nValue) || !MoneyRange(nValueIn)) {
                        return state.DoS(100, false, REJECT_INVALID, "bad-txns-inputvalues-outofrange");
                    }
                }

                const asset_tag::CAmountMap value_out = tx.GetValueOut();
                if (nValueIn < value_out) {
                    return state.DoS(100, false, REJECT_INVALID, "bad-txns-in-belowout", false,
                        strprintf("value in (%s) < value out (%s)", FormatMoney(nValueIn), FormatMoney(value_out)));
                }

                // Tally transaction fees
                // TODO NOKUBIT: Nokubit AssetCoin   DA VALUTARE se gli Asset si possono perdere
                // TODO NOKUBIT: Nokubit MemPool    EVENTUALMENTE CON TXO PARTICOLARE
                asset_tag::CAmountMap txfeemap_aux = nValueIn - value_out;
                if (!MoneyRangeAssetZero(txfeemap_aux)) {
                    return state.DoS(100, false, REJECT_INVALID, "bad-txns-fee-outofrange");
                }
// TODO NOKUBIT: log
LogPrint(BCLog::ASSET, "\tParse: %s\n", FormatMoney(value_out));

                //txfee = txfeemap_aux[asset_tag::COIN_ASSET];
            }
        }
    }

    organizations.Dump();
    names.Dump();

//         modtxscoreiter modit = mapModifiedTx.get<ancestor_score>().begin();
//         if (mi == mempool.mapTx.get<ancestor_score>().end()) {
//             // We're out of entries in mapTx; use the entry from mapModifiedTx
//             iter = modit->iter;
//             fUsingModified = true;
//         } else {
//             // Try to compare the mapTx entry to the mapModifiedTx entry
//             iter = mempool.mapTx.project<0>(mi);
//             if (modit != mapModifiedTx.get<ancestor_score>().end() &&
//                     CompareTxMemPoolEntryByAncestorFee()(*modit, CTxMemPoolModifiedEntry(iter))) {
//                 // The best entry in mapModifiedTx has higher score
//                 // than the one from mapTx.
//                 // Switch which transaction (package) to consider
//                 iter = modit->iter;
//                 fUsingModified = true;
//             } else {
//                 // Either no entry in mapModifiedTx, or it's worse than mapTx.
//                 // Increment mi for the next loop iteration.
//                 ++mi;
//             }
//         }

//         // We skip mapTx entries that are inBlock, and mapModifiedTx shouldn't
//         // contain anything that is inBlock.
//         assert(!inBlock.count(iter));

//         // Package can be added. Sort the entries in a valid order.
//         std::vector<CTxMemPool::txiter> sortedEntries;
//         SortForBlock(ancestors, iter, sortedEntries);

// // TODO NOKUBIT: log
// LogPrint(BCLog::ASSET, "\taddPackageTxs: AddToBlock\n");
//         for (size_t i=0; i<sortedEntries.size(); ++i) {
//             // TODO NOKUBIT: escludi ref tag
//             //  Questo test non è più necessario per come viene inserito il MainRef
//             //  Decidere se togliere
//             if (sortedEntries[i]->GetTx().IsMainRef()) {
// // TODO NOKUBIT: log
// LogPrint(BCLog::ASSET, "\taddPackageTxs: Excluded %s\n", sortedEntries[i]->GetTx().ToString());
//                 continue;
//             }
//             AddToBlock(sortedEntries[i]);
//             // Erase from the modified set, if present
//             mapModifiedTx.erase(sortedEntries[i]);
//         }

//         ++nPackagesSelected;


    int64_t mid = GetTimeMicros();

    try {
        FILE* filestr = fsbridge::fopen(GetDataDir() / "fee-sharing.dat.new", "wb");
        if (!filestr) {
            return false;
        }

        CAutoFile file(filestr, SER_DISK, CLIENT_VERSION);

        uint64_t version = FEE_SHARING_DUMP_VERSION;
        file << version;

        file << (uint64_t)vinfo.size();
        // for (const auto& i : vinfo) {
        //     file << *(i.tx);
        //     file << (int64_t)i.nTime;
        //     file << (int64_t)i.nFeeDelta;
        //     mapDeltas.erase(i.tx->GetHash());
        // }

        FileCommit(file.Get());
        file.fclose();
        RenameOver(GetDataDir() / "fee-sharing.dat.new", GetDataDir() / "fee-sharing.dat");
        int64_t last = GetTimeMicros();
        LogPrintf("Dumped fee-sharing: %gs to copy, %gs to dump\n", (mid-start)*MICRO, (last-mid)*MICRO);
    } catch (const std::exception& e) {
        LogPrintf("Failed to dump fee-sharing: %s. Continuing anyway.\n", e.what());
        return false;
    }
    return true;
}


bool CheckTagBlockTransaction(const CTransaction& tx, CValidationState &state, bool fCheckDuplicateInputs)
{
    // Basic checks that don't depend on any context
    if (tx.vin.empty())
        return state.DoS(10, false, REJECT_INVALID, "bad-txns-vin-empty");
    if (tx.vout.empty())
        return state.DoS(10, false, REJECT_INVALID, "bad-txns-vout-empty");
    // Size limits (this doesn't take the witness into account, as that hasn't been checked for malleability)
    //if (::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * WITNESS_SCALE_FACTOR > MAX_BLOCK_WEIGHT)
    //    return state.DoS(100, false, REJECT_INVALID, "bad-txns-oversize");

    if (tx.IsCoinBase())
    {
        if (tx.vin[0].scriptSig.size() < 2 || tx.vin[0].scriptSig.size() > 100)
            return state.DoS(100, false, REJECT_INVALID, "bad-cb-length");
    }
    else
    {
        return state.DoS(10, false, REJECT_INVALID, "bad-txns-tagblock");
    }

    // NOKUBIT: Nokubit verifica formale PegIns e Asset
    if (tx.HasTag()) {
        std::string error;
        if (!tx.CheckTagFormat(error)) {
            return state.Invalid(false, REJECT_INVALID, error);
        }
// TODO NOKUBIT: log
LogPrint(BCLog::ASSET, "\tCheckTransaction: Tag format OK\n");
    }

    // Check for negative or overflow output values
    // NOKUBIT: Nokubit AssetCoin
    asset_tag::CAmountMap nValueOut;
    for (const auto& txout : tx.vout)
    {
        if (txout.nValue < 0)
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-vout-negative");
        if (txout.nValue > MAX_MONEY)
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-vout-toolarge");
        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut))
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-txouttotal-toolarge");
    }

    // Check for duplicate inputs - note that this check is slow so we skip it in CheckBlock
    if (fCheckDuplicateInputs) {
        std::set<COutPoint> vInOutPoints;
        for (const auto& txin : tx.vin)
        {
            if (!vInOutPoints.insert(txin.prevout).second)
                return state.DoS(100, false, REJECT_INVALID, "bad-txns-inputs-duplicate");
        }
    }

    return true;
}


}   // block_tag
