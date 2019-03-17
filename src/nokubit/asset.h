// TODO NOKUBIT: header


// Copyright (c) 2017-2018 Noku developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NOKUBIT_ASSET_H
#define BITCOIN_NOKUBIT_ASSET_H

#include <amount.h>
#include <script/script.h>
#include <uint256.h>
#include <hash.h>
#include <vector>
#include <stdint.h>
#include <string>

// TODO NOKUBIT: per log
#include <util.h>
#include <utilstrencodings.h>

class UniValue;
class Coin;
class COutPoint;
class CTransaction;
class CTxOutCompressor;

namespace asset_tag {

static const size_t ASSET_NAME_MIN_SIZE = 3;
static const size_t ASSET_NAME_MAX_SIZE = 12;
static const size_t ASSET_DESCRIPTION_MAX_SIZE = 64;

static const size_t ORGANIZATION_NAME_MIN_SIZE = 3;
static const size_t ORGANIZATION_NAME_MAX_SIZE = 32;
static const size_t ORGANIZATION_DESCRIPTION_MAX_SIZE = 255;

typedef std::string AssetName;
static const std::vector<AssetName> ASSET_NAME_RESERVED = {"noku", "nokubit"};
static const AssetName COIN_ASSET = AssetName("");

typedef std::map<AssetName, CAmount> CAmountMap;

class AssetAmount {
private:
    AssetName name;
    CAmount nValue;

public:
    AssetAmount() { SetNull(); }
    explicit AssetAmount(const AssetName& nameIn, const CAmount& nValueIn) : name(nameIn.substr(0, ASSET_NAME_MAX_SIZE)), nValue(nValueIn) {};
    explicit AssetAmount(const std::pair<AssetName, CAmount>& assetAmountIn) : name(assetAmountIn.first.substr(0, ASSET_NAME_MAX_SIZE)), nValue(assetAmountIn.second) {};

    friend CTxOutCompressor;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(LIMITED_STRING(name, ASSET_NAME_MAX_SIZE));
        READWRITE(nValue);
    }

    // FOR static void ApplyStats(CCoinsStats &stats, CHashWriter& ss, const uint256& hash, const std::map<uint32_t, Coin>& outputs);
    static void AssetApplyStats(CHashWriter& ss, const AssetAmount& asset) {
        ss << asset.name;
        ss << VARINT(asset.nValue);
    }

    void SetNull()
    {
        nValue = -1;
        name = COIN_ASSET;
    }

    bool IsNull() const
    {
        return (nValue == -1);
    }

    AssetName GetAmountName() const { return name; }
    CAmount GetAmountValue() const { return nValue; }
    CAmountMap GetAmountMap() const { return {{name, nValue}}; }

    inline bool operator==(const AssetAmount& b) const { return (name == b.name && nValue == b.nValue); }
    inline bool operator!=(const AssetAmount& b) const { return (name != b.name || nValue != b.nValue); }
    inline bool operator>(const AssetAmount& b) const { return (name == b.name && nValue > b.nValue) || (name != b.name && nValue > 0); }
    inline bool operator>=(const AssetAmount& b) const { return (name == b.name && nValue >= b.nValue) || (name != b.name && nValue >= 0); }
    inline bool operator<(const AssetAmount& b) const { return (name == b.name && nValue < b.nValue) || (name != b.name && nValue < 0); }
    inline bool operator<=(const AssetAmount& b) const { return (name == b.name && nValue <= b.nValue) || (name != b.name && nValue <= 0); }

    inline bool operator==(const CAmount& v) const { return nValue == v; }
    inline bool operator==(const AssetName& s) const { return name == s; }
    inline bool operator!=(const CAmount& v) const { return nValue != v; }
    inline bool operator!=(const AssetName& s) const { return name != s; }
    inline bool operator>(const CAmount& b) const { return nValue > b; }
    inline bool operator>=(const CAmount& b) const { return nValue >= b; }
    inline bool operator<(const CAmount& b) const { return nValue < b; }
    inline bool operator<=(const CAmount& b) const { return nValue <= b; }

    inline AssetAmount& operator+=(const CAmount& b)
    {
        nValue += b;
        return *this;
    }

    inline AssetAmount& operator-=(const CAmount& b)
    {
        nValue -= b;
        return *this;
    }

    friend CAmountMap& operator+=(CAmountMap& a, const AssetAmount& b)
    {
        a[b.name] += b.nValue;
        return a;
    }

    friend CAmountMap& operator-=(CAmountMap& a, const AssetAmount& b)
    {
        a[b.name] -= b.nValue;
        return a;
    }

    friend CAmountMap operator+(const CAmountMap& a, const AssetAmount& b)
    {
        CAmountMap c;
        for (CAmountMap::const_iterator it = a.begin(); it != a.end(); it++) {
            if (it->second != 0)
                c[it->first] = it->second;
        }
        c[b.name] += b.nValue;
        return c;
    }

    friend CAmountMap operator-(const CAmountMap& a, const AssetAmount& b)
    {
        CAmountMap c;
        for (CAmountMap::const_iterator it = a.begin(); it != a.end(); it++) {
            if (it->second != 0)
                c[it->first] = it->second;
        }
        c[b.name] -= b.nValue;
        return c;
    }

    std::string ToString() const
    {
        if (name != COIN_ASSET) {
            return strprintf("AssetAmount(nValue=%d, name=%s)", nValue, name);
        }
        return strprintf("Money(%d.%08d)", nValue / COIN, nValue % COIN);
    }
};

class ScriptTag;
static const CScriptWitness scriptWitnessEmpty;
static const unsigned int MAX_TAG_WITNESS_STACK_ITEM = 3;

static const std::string NKB_TAG_PREFIX("nkb");

enum NkbTagIdentifier: uint8_t {
    NkbNone = 0,
    NkbPoS = 's',
    NkbReference = 'r',
    NkbPegin = 'p',
    NkbOrganization = 'o',
    NkbMintAsset = 'm',
    NkbAsset = 'a',
};

class AssetTag
{
private:
    const NkbTagIdentifier tagType;
    const CScriptWitness& scriptWitness;
    friend class ScriptTag;
    mutable ScriptTag const * tag_ptr = nullptr;

    AssetTag(const NkbTagIdentifier type, const CScriptWitness &scriptWitness)
        : tagType(type), scriptWitness(scriptWitness) { }

    AssetTag(const NkbTagIdentifier type, const ScriptTag *ptag)
        : tagType(type), scriptWitness(scriptWitnessEmpty), tag_ptr(ptag) { }

    inline void extract() const;

public:
    AssetTag() : tagType(NkbNone), scriptWitness(scriptWitnessEmpty) { }
    ~AssetTag();

    bool IsNull() const { return tagType == NkbNone; }
    NkbTagIdentifier getTagType() const { return tagType; }
    bool IsTagType(NkbTagIdentifier tag) const { return tagType == tag; }
    bool CheckTag(const CTransaction& tx, std::string &error) const;
    bool EnforceTag() const;
    bool IsValidate() const;
    bool IsReplaceable(const Coin& coin) const;

    bool operator==(AssetTag& o) const;
    bool operator!=(AssetTag& o) const;

    void GetTagOutpoint(COutPoint& out) const;
    void GetTagCoin(Coin& coin, int height) const;
    AssetAmount GetAmount() const;
    void ToUniv(UniValue& uv) const;
    std::string ToString() const;

    static AssetTag* ExtractTag(const CScriptWitness &scriptWitness)
    {
        // TODO NOKUBIT: testare il vettore
        if (scriptWitness.stack.size() >= 2 && scriptWitness.stack.size() <= MAX_TAG_WITNESS_STACK_ITEM &&
            scriptWitness.stack[0].size() > 4) {
            const std::vector<unsigned char>& vch = scriptWitness.stack[0];
            const std::string prefix((char *)vch.data(), 3);
            if (prefix == NKB_TAG_PREFIX) {
                unsigned char pegType = vch[3];
                switch (pegType)
                {
                    // TODO NOKUBIT:
                    case NkbReference:
                    case NkbPegin:
                    case NkbOrganization:
                    case NkbMintAsset:
                    case NkbAsset:
                        return new AssetTag((NkbTagIdentifier)pegType, scriptWitness);
                    case NkbNone:
                    default:
LogPrint(BCLog::ASSET, "\tExtractTag UNKNOWN type: %s\n", HexStr(vch.begin(), vch.begin() + 4));
                        break;
                }
            } else
LogPrint(BCLog::ASSET, "\tExtractTag INVALID pfx: %s\n", HexStr(vch.begin(), vch.begin() + 4));
        } else
LogPrint(BCLog::ASSET, "\tExtractTag ERROR pfx: %d\n", scriptWitness.stack.size());
            return new AssetTag();
    }
};

class PoSScript;

class PoSTag
{
private:
    const CScriptWitness scriptWitness;
    const uint256 headerHash;
    friend class PoSScript;
    mutable PoSScript const *pos_ptr = nullptr;

    PoSTag(const CScriptWitness& scriptWitness, const uint256& hash)
        : scriptWitness(scriptWitness), headerHash(hash) { }

    inline bool extract() const;

public:
    PoSTag() = delete;
    PoSTag(const uint256& hash) : headerHash(hash) { }
    ~PoSTag();

    bool IsNull() const { return scriptWitness.IsNull(); }
    const CScriptWitness& GetScript() const { return scriptWitness; }
    bool IsValidate() const;

    //bool operator==(PoSTag& o) const;
    //bool operator!=(PoSTag& o) const;

    const uint256 GetPoSHash() const;
    AssetAmount GetAmount() const;
    void ToUniv(UniValue& uv) const;
    std::string ToString() const;

    static PoSTag* ExtractTag(const CScriptWitness& scriptWitness, const uint256& hash)
    {
        if (scriptWitness.IsNull())
            return new PoSTag(hash);
        if (scriptWitness.stack.size() == 2 && scriptWitness.stack[0].size() > 4) {
            const std::vector<unsigned char>& vch = scriptWitness.stack[0];
            const std::string prefix((char *)vch.data(), 3);
            if (prefix == NKB_TAG_PREFIX && vch[3] == NkbPoS) {
                return new PoSTag(scriptWitness, hash);
            }
LogPrint(BCLog::ASSET, "\tExtractTag INVALID pfx: %s\n", HexStr(vch.begin(), vch.begin() + 4));
        }
        else
LogPrint(BCLog::ASSET, "\tExtractTag NO POS: %d\n", scriptWitness.stack.size());
        return new PoSTag(hash);
    }
};

}   // asset_tag


inline asset_tag::CAmountMap& operator+=(asset_tag::CAmountMap& a, const asset_tag::CAmountMap& b)
{
    for (asset_tag::CAmountMap::const_iterator it = b.begin(); it != b.end(); it++) {
        a[it->first] += it->second;
    }
    return a;
}

inline asset_tag::CAmountMap& operator-=(asset_tag::CAmountMap& a, const asset_tag::CAmountMap& b)
{
    for (asset_tag::CAmountMap::const_iterator it = b.begin(); it != b.end(); it++) {
        a[it->first] -= it->second;
    }
    return a;
}

inline asset_tag::CAmountMap operator+(const asset_tag::CAmountMap& a, const asset_tag::CAmountMap& b)
{
    asset_tag::CAmountMap c;
    for (asset_tag::CAmountMap::const_iterator it = a.begin(); it != a.end(); it++) {
        if (it->second != 0)
            c[it->first] = it->second;
    }
    for (asset_tag::CAmountMap::const_iterator it = b.begin(); it != b.end(); it++) {
        c[it->first] += it->second;
    }
    return c;
}

inline asset_tag::CAmountMap operator-(const asset_tag::CAmountMap& a, const asset_tag::CAmountMap& b)
{
    asset_tag::CAmountMap c;
    for (asset_tag::CAmountMap::const_iterator it = a.begin(); it != a.end(); it++) {
        if (it->second != 0)
            c[it->first] = it->second;
    }
    for (asset_tag::CAmountMap::const_iterator it = b.begin(); it != b.end(); it++) {
        c[it->first] -= it->second;
    }
    return c;
}

inline asset_tag::CAmountMap operator-(const asset_tag::CAmountMap& a)
{
    asset_tag::CAmountMap c;
    for (asset_tag::CAmountMap::const_iterator it = a.begin(); it != a.end(); it++) {
        if (it->second != 0)
            c[it->first] = -it->second;
    }
    return c;
}

namespace amount_map
{
    //  operator overloading is too critical
    inline bool cmpAllGreat(const asset_tag::CAmountMap& m, const CAmount& v)
    {
        for (asset_tag::CAmountMap::const_iterator it = m.begin(); it != m.end(); it++) {
            if (m.at(it->first) <= v)
                return false;
        }
        return true;
    }
    inline bool cmpOneGreat(const asset_tag::CAmountMap& m, const CAmount& v)
    {
        for (asset_tag::CAmountMap::const_iterator it = m.begin(); it != m.end(); it++) {
            if (m.at(it->first) > v)
                return true;
        }
        return false;
    }
    inline bool cmpAllGreatEq(const asset_tag::CAmountMap& m, const CAmount& v)
    {
        for (asset_tag::CAmountMap::const_iterator it = m.begin(); it != m.end(); it++) {
            if (m.at(it->first) < v)
                return false;
        }
        return true;
    }
    inline bool cmpOneGreatEq(const asset_tag::CAmountMap& m, const CAmount& v)
    {
        for (asset_tag::CAmountMap::const_iterator it = m.begin(); it != m.end(); it++) {
            if (m.at(it->first) >= v)
                return true;
        }
        return false;
    }
}   // amount_map

// inline bool operator<(const asset_tag::CAmountMap& a, const asset_tag::CAmountMap& b)
// {
//     for(asset_tag::CAmountMap::const_iterator it = b.begin(); it != b.end(); ++it) {
//         CAmount aValue = a.count(it->first) ? a.find(it->first)->second : 0;
//         if (aValue < it->second)
//             return true;
//     }
//     return false;
// }

// bool operator<=(const CAmountMap& a, const CAmountMap& b);
// bool operator>(const CAmountMap& a, const CAmountMap& b);
// bool operator>=(const CAmountMap& a, const CAmountMap& b);
// bool operator==(const CAmountMap& a, const CAmountMap& b);
// bool operator!=(const CAmountMap& a, const CAmountMap& b);

// inline bool operator==(const asset_tag::CAmountMap& a, const CAmount& b)
// {
//     for (asset_tag::CAmountMap::const_iterator it = a.begin(); it != a.end(); it++) {
//         if (a.at(it->first) != b)
//             return false;
//     }
//     return true;
// }


inline bool MoneyRange(const asset_tag::AssetAmount& amount) {
    if (amount < 0 || amount > MAX_MONEY)
        return false;
    return true;
}

inline bool MoneyRange(const asset_tag::CAmountMap& mapValue) {
    for (asset_tag::CAmountMap::const_iterator it = mapValue.begin(); it != mapValue.end(); it++)
        if (!MoneyRange(it->second))
            return false;
    return true;
}

inline bool MoneyRangeAssetZero(const asset_tag::CAmountMap& mapValue) {
    for (asset_tag::CAmountMap::const_iterator it = mapValue.begin(); it != mapValue.end(); it++) {
        if (it->first != asset_tag::COIN_ASSET) {
            if (it->second != 0)
                return false;
        }
        else if (!MoneyRange(it->second))
            return false;
    }
    return true;
}


#endif //  BITCOIN_NOKUBIT_ASSET_H
