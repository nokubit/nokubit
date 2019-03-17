// TODO NOKUBIT: header


// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NOKUBIT_ASSETDB_H
#define BITCOIN_NOKUBIT_ASSETDB_H

#include <nokubit/tagscript.h>
#include <script/standard.h>
#include <fs.h>
#include <serialize.h>

#include <string>
#include <map>


class CDataStream;

namespace asset_db {

class Organization;
class AssetName;

/**
 * A Organization entry.
 *
 */
class OrganizationEntry
{
public:
    static const unsigned char ENTRY_VERSION = 1;

    asset_tag::OrganizationScript entry;
    int nHeight;

    OrganizationEntry()
    {
        SetNull();
    }

    explicit OrganizationEntry(asset_tag::OrganizationScript&& entry, int nHeight) : 
        entry(std::move(entry)), nHeight(nHeight) {}

    bool IsEnabled() const {
        return entry.enableFlag != 0;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(entry);
        READWRITE(VARINT(nHeight));
    }

    void SetNull()
    {
        entry.SetNull();
        nHeight = -1;
    }
};


/**
 * A Asset entry.
 *
 */
class AssetNameEntry
{
public:
    static const unsigned char ENTRY_VERSION = 1;

    asset_tag::MintAssetScript entry;
    int nHeight;

    AssetNameEntry()
    {
        SetNull();
    }

    explicit AssetNameEntry(asset_tag::MintAssetScript&& entry, int nHeight) :
        entry(std::move(entry)), nHeight(nHeight) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(entry);
        READWRITE(VARINT(nHeight));
    }

    void SetNull()
    {
        entry.SetNull();
        nHeight = -1;
    }
};

/** Access to the (Organization/State) address database (states.dat) */
class OrganizationDB
{
private:
    fs::path pathState;

public:
    OrganizationDB();
    bool Write(const Organization& data);
    bool Read(Organization& data);
};

/** Access to the Asset Names database (assets.dat) */
class AssetNameDB
{
private:
    fs::path pathAsset;
public:
    AssetNameDB();
    bool Write(const AssetName& data);
    bool Read(AssetName& data);
};

typedef std::map<WitnessV0KeyHash, std::vector<CAmount>> afeemap_t;

/** Access to the Asset Fee bucket database (assetfee.dat) */
class AssetFeeDB
{
private:
    fs::path pathFees;
public:
    AssetFeeDB();
    bool Write(const afeemap_t& feeSet);
    bool Read(afeemap_t& feeSet);
};

}   // asset_db

#endif // BITCOIN_NOKUBIT_ASSETDB_H
