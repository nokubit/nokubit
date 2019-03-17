// TODO NOKUBIT: header


// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NOKUBIT_ORGANIZATION_H
#define BITCOIN_NOKUBIT_ORGANIZATION_H

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

class Organization
{
private:
    mutable CCriticalSection cs;
    std::vector<OrganizationEntry> vEntries;
    std::map<std::string, std::vector<size_t>> stateByName;
    std::map<uint160, std::vector<size_t>> stateByAddress;
    bool dirty;
    bool loaded;
    OrganizationDB db;

    void Clear()
    {
        LOCK(cs);
        stateByName.clear();
        stateByAddress.clear();
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
        stateByName.clear();
        stateByAddress.clear();
        for (size_t i = 0; i < vEntries.size(); i++)
        {
            const OrganizationEntry& og = vEntries[i];
            // auto in = stateByName.emplace(std::piecewise_construct, std::make_tuple(og.entry.organizationName), std::tuple<>()).first;
            // in->second.push_back(i);
            // auto ia = stateByAddress.emplace(std::piecewise_construct, std::make_tuple(og.entry.organizationAddress), std::tuple<>()).first;
            // ia->second.push_back(i);
            auto in = stateByName.emplace(og.entry.organizationName, std::vector<size_t>()).first;
            in->second.emplace_back(i);
            auto ia = stateByAddress.emplace(og.entry.organizationAddress, std::vector<size_t>()).first;
            ia->second.emplace_back(i);
        }
    }

public:
    Organization() { Clear(); };

    template<typename Stream>
    void Serialize(Stream &s) const
    {
        LOCK(cs);

        s << OrganizationEntry::ENTRY_VERSION;
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
        if (nVersion != OrganizationEntry::ENTRY_VERSION)
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

    void Add(const asset_tag::OrganizationScript& scriptIn, int nHeight)
    {
        if (!loaded)
            Load();
        asset_tag::OrganizationScript mScript = scriptIn;
        const OrganizationEntry entry(std::move(mScript), nHeight);
        LOCK(cs);
        int idx = vEntries.size();
        auto in = stateByName.emplace(scriptIn.organizationName, std::vector<size_t>()).first;
        in->second.emplace_back(idx);
        auto ia = stateByAddress.emplace(scriptIn.organizationAddress, std::vector<size_t>()).first;
        ia->second.emplace_back(idx);

        vEntries.push_back(entry);
        dirty = true;
    }
};

}   // asset_db

#endif // BITCOIN_NOKUBIT_ORGANIZATION_H
