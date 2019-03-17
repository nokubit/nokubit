// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PRIMITIVES_BLOCK_H
#define BITCOIN_PRIMITIVES_BLOCK_H

#include <primitives/transaction.h>
#include <serialize.h>
#include <uint256.h>
// NOKUBIT: POS&FEE
#include <script/script.h>
#include <nokubit/asset.h>

// NOKUBIT: POS&FEE
static const int SERIALIZE_HEADER_POS = 0x20000000;


/** Nodes collect new transactions into a block, hash them into a hash tree,
 * and scan through nonce values to make the block's hash satisfy proof-of-work
 * requirements.  When they solve the proof-of-work, they broadcast the block
 * to everyone and the block is added to the block chain.  The first transaction
 * in the block is a special one that creates a new coin owned by the creator
 * of the block.
 */
class CBlockHeader
{
    // NOKUBIT: POS&FEE
    friend class CBlock;
    /** Memory and network only. */
    mutable asset_tag::PoSTag* tagPoS = nullptr;    //! Only serialized through CTransaction

public:
    // header
    int32_t nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nNonce;

    CBlockHeader()
    {
        SetNull();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(this->nVersion);
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);

        // NOKUBIT: POS&FEE
        if ((s.GetVersion() & SERIALIZE_HEADER_POS)) {
            CScriptWitness script;
            if (tagPoS != nullptr)
                script = tagPoS->GetScript();
            READWRITE(script.stack);
            if (ser_action.ForRead()) {
                tagPoS = asset_tag::PoSTag::ExtractTag(script, GetHash());
            }
        }
    }

    // NOKUBIT: POS&FEE
    CScriptWitness GetPoSSerialization() const
    {
        if (tagPoS == nullptr)
            return CScriptWitness();
        return tagPoS->GetScript();
    }

    // NOKUBIT: POS&FEE
    void InitPoS(const CScriptWitness& script) const
    {
        if (tagPoS == nullptr) {
            tagPoS = asset_tag::PoSTag::ExtractTag(script, GetHash());
        }
    }

    void SetNull()
    {
        nVersion = 0;
        hashPrevBlock.SetNull();
        hashMerkleRoot.SetNull();
        nTime = 0;
        nBits = 0;
        nNonce = 0;
        // NOKUBIT: POS&FEE
        tagPoS = nullptr;
    }

    bool IsNull() const
    {
        return (nBits == 0);
    }

    // NOKUBIT: POS&FEE
    bool IsPoSParsed() const
    {
        return tagPoS != nullptr;
    }

    // NOKUBIT: POS&FEE
    bool IsPoSValid() const
    {
        return tagPoS != nullptr && tagPoS->IsValidate();
    }

    // NOKUBIT: POS&FEE
    // Compute at which vout of the block's coinbase transaction the PoS
    // commitment occurs, return witness.
    bool GetPoSCommitment(const CTransaction& tx, CScriptWitness& commit) const
    {
        if (!tx.IsCoinBase() || tx.vin[0].scriptWitness.stack.size() != 2)
            return false;
        for (size_t o = 0; o < tx.vout.size(); o++) {
            const CScript& script = tx.vout[o].scriptPubKey;
            CScript::const_iterator pc = script.begin();
            std::vector<unsigned char> data;
            opcodetype opcode;
            if (!script.GetOp(pc, opcode, data) || opcode != OP_RETURN)
                continue;
            if (script.GetOp(pc, opcode, data) && data.size() > 4 && pc == script.end()) {
                const std::string prefix((char *)data.data(), 3);
                if (prefix == asset_tag::NKB_TAG_PREFIX && data[3] == asset_tag::NkbTagIdentifier::NkbPoS) {
                    commit.stack.resize(2);
                    commit.stack[0] = data;
                    commit.stack[1] = tx.vin[0].scriptWitness.stack[1];
                    return true;
                }
            }
        }
        return false;
    }

    uint256 GetHash() const;

    // NOKUBIT: POS&FEE
    uint256 GetPoSHash() const
    {
        if (tagPoS == nullptr || tagPoS->IsNull())
            return GetHash();
        return tagPoS->GetPoSHash();
    }

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }
};


class CBlock : public CBlockHeader
{
public:
    // network and disk
    std::vector<CTransactionRef> vtx;

    // memory only
    mutable bool fChecked;

    CBlock()
    {
        SetNull();
    }

    CBlock(const CBlockHeader &header)
    {
        SetNull();
        *((CBlockHeader*)this) = header;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(*(CBlockHeader*)this);
        READWRITE(vtx);
        // NOKUBIT: POS&FEE
        if (ser_action.ForRead()) {
            CScriptWitness script;
            GetPoSCommitment(script);
            InitPoS(script);
        }
    }

    void SetNull()
    {
        CBlockHeader::SetNull();
        vtx.clear();
        fChecked = false;
    }

    // NOKUBIT: POS&FEE
    bool GetPoSCommitment(CScriptWitness& commit) const
    {
        if (vtx.empty())
            return false;
        const CTransaction& tx = *(vtx[0]);
        return CBlockHeader::GetPoSCommitment(tx, commit);
    }

    CBlockHeader GetBlockHeader() const
    {
        CBlockHeader block;
        block.nVersion       = nVersion;
        block.hashPrevBlock  = hashPrevBlock;
        block.hashMerkleRoot = hashMerkleRoot;
        block.nTime          = nTime;
        block.nBits          = nBits;
        block.nNonce         = nNonce;

        // TODO NOKUBIT: POS&FEE
        block.tagPoS         = tagPoS;
        return block;
    }

    std::string ToString() const;
};

/** Describes a place in the block chain to another node such that if the
 * other node doesn't have the same branch, it can find a recent common trunk.
 * The further back it is, the further before the fork it may be.
 */
struct CBlockLocator
{
    std::vector<uint256> vHave;

    CBlockLocator() {}

    explicit CBlockLocator(const std::vector<uint256>& vHaveIn) : vHave(vHaveIn) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vHave);
    }

    void SetNull()
    {
        vHave.clear();
    }

    bool IsNull() const
    {
        return vHave.empty();
    }
};

#endif // BITCOIN_PRIMITIVES_BLOCK_H
