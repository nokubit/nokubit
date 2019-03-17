// TODO NOKUBIT: header


// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NOKUBIT_ASSETNAME_H
#define BITCOIN_NOKUBIT_ASSETNAME_H

#include <nokubit/assetdb.h>
//+- #include <primitives/transaction.h>
//+- #include <compressor.h>
//+- #include <core_memusage.h>
//+- #include <hash.h>
//+- #include <memusage.h>
#include <serialize.h>
//+- #include <uint256.h>

//+- #include <assert.h>
#include <stdint.h>



namespace asset_db {

class AssetName
{
private:
    mutable CCriticalSection cs;
    std::vector<AssetNameEntry> vEntries;
    std::map<std::string, std::vector<size_t>> assetByName;
    std::map<uint160, std::vector<size_t>> assetByAddress;
    bool dirty;
    bool loaded;
    AssetNameDB db;

    void Clear()
    {
        LOCK(cs);
        assetByName.clear();
        assetByAddress.clear();
        vEntries.clear();
        dirty = false;
        loaded = false;
    }

    void Load()
    {
        db.Read(*this);
        loaded = true;
    }

    void Write()
    {
        db.Write(*this);
        dirty = false;
    }

    void Map()
    {
        LOCK(cs);
        assetByName.clear();
        assetByAddress.clear();
        for (size_t i = 0; i < vEntries.size(); i++)
        {
            const AssetNameEntry& og = vEntries[i];
            auto in = assetByName.emplace(og.entry.assetName, std::vector<size_t>()).first;
            in->second.emplace_back(i);
            auto ia = assetByAddress.emplace(og.entry.GetAddress(), std::vector<size_t>()).first;
            ia->second.emplace_back(i);
        }
    }

public:
    AssetName() { Clear(); };

    template<typename Stream>
    void Serialize(Stream &s) const
    {
        LOCK(cs);

        s << AssetNameEntry::ENTRY_VERSION;
        s << vEntries;
    }

    template<typename Stream>
    void Unserialize(Stream& s)
    {
        LOCK(cs);

        Clear();
        unsigned char nVersion;
        s >> nVersion;
        // ... trap for future change
        if (nVersion != AssetNameEntry::ENTRY_VERSION)
            error("%s: Invalid version number", __func__);
        s >> vEntries;
        Map();
        dirty = false;
    }

    void Dump() {
        LOCK(cs);
        if (dirty)
            Write();
    }

    void Add(const asset_tag::MintAssetScript& scriptIn, int nHeight)
    {
        if (!loaded)
            Load();
        asset_tag::MintAssetScript mScript = scriptIn;
        const AssetNameEntry entry(std::move(mScript), nHeight);
        LOCK(cs);
        int idx = vEntries.size();
        auto in = assetByName.emplace(scriptIn.assetName, std::vector<size_t>()).first;
        in->second.emplace_back(idx);
        auto ia = assetByAddress.emplace(scriptIn.GetAddress(), std::vector<size_t>()).first;
        ia->second.emplace_back(idx);

        vEntries.push_back(entry);
        dirty = true;
    }
};

}   // asset_db

#endif // BITCOIN_NOKUBIT_ORGANIZATION_H
