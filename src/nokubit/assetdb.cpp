// TODO NOKUBIT: header


// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <nokubit/assetdb.h>
#include <nokubit/organization.h>
#include <nokubit/assetname.h>

#include <chainparams.h>
#include <clientversion.h>
//+- #include <hash.h>
#include <random.h>
#include <streams.h>
//+- #include <tinyformat.h>
//+- #include <util.h>
//+- #include <pow.h>
//+- #include <uint256.h>
//+- #include <util.h>
//+- #include <ui_interface.h>
//+- #include <init.h>
//+-
//+- #include <stdint.h>

namespace asset_db {

template <typename Stream, typename Data>
bool SerializeDB(Stream& stream, const Data& data)
{
    // Write and commit header, data
    try {
        CHashWriter hasher(SER_DISK, CLIENT_VERSION);
        stream << FLATDATA(Params().MessageStart()) << data;
        hasher << FLATDATA(Params().MessageStart()) << data;
        stream << hasher.GetHash();
    } catch (const std::exception& e) {
        return error("%s: Serialize or I/O error - %s", __func__, e.what());
    }

    return true;
}

template <typename Data>
bool SerializeFileDB(const std::string& prefix, const fs::path& path, const Data& data)
{
    // Generate random temporary filename
    unsigned short randv = 0;
    GetRandBytes((unsigned char*)&randv, sizeof(randv));
    std::string tmpfn = strprintf("%s.%04x", prefix, randv);

    // open temp output file, and associate with CAutoFile
    fs::path pathTmp = GetDataDir() / tmpfn;
    FILE *file = fsbridge::fopen(pathTmp, "wb");
    CAutoFile fileout(file, SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("%s: Failed to open file %s", __func__, pathTmp.string());

    // Serialize
    if (!SerializeDB(fileout, data)) return false;
    FileCommit(fileout.Get());
    fileout.fclose();

    // replace existing file, if any, with new file
    if (!RenameOver(pathTmp, path))
        return error("%s: Rename-into-place failed", __func__);

    return true;
}

template <typename Stream, typename Data>
bool DeserializeDB(Stream& stream, Data& data, bool fCheckSum = true)
{
    try {
        CHashVerifier<Stream> verifier(&stream);
        // de-serialize file header (network specific magic number) and ..
        unsigned char pchMsgTmp[4];
        verifier >> FLATDATA(pchMsgTmp);
        // ... verify the network matches ours
        if (memcmp(pchMsgTmp, Params().MessageStart(), sizeof(pchMsgTmp)))
            return error("%s: Invalid network magic number", __func__);

        // de-serialize data
        verifier >> data;

        // verify checksum
        if (fCheckSum) {
            uint256 hashTmp;
            stream >> hashTmp;
            if (hashTmp != verifier.GetHash()) {
                return error("%s: Checksum mismatch, data corrupted", __func__);
            }
        }
    }
    catch (const std::exception& e) {
        return error("%s: Deserialize or I/O error - %s", __func__, e.what());
    }

    return true;
}

template <typename Data>
bool DeserializeFileDB(const fs::path& path, Data& data)
{
    // open input file, and associate with CAutoFile
    FILE *file = fsbridge::fopen(path, "rb");
    CAutoFile filein(file, SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
        return error("%s: Failed to open file %s", __func__, path.string());

    return DeserializeDB(filein, data);
}

OrganizationDB::OrganizationDB()
{
    pathState = GetDataDir() / "states.dat";
}

bool OrganizationDB::Write(const Organization& data)
{
    return SerializeFileDB("states", pathState, data);
}

bool OrganizationDB::Read(Organization& data)
{
    return DeserializeFileDB(pathState, data);
}

AssetFeeDB::AssetFeeDB()
{
    pathFees = GetDataDir() / "assetfee.dat";
}

bool AssetFeeDB::Write(const afeemap_t& feeSet)
{
    return SerializeFileDB("assetfee", pathFees, feeSet);
}

bool AssetFeeDB::Read(afeemap_t& feeSet)
{
    return DeserializeFileDB(pathFees, feeSet);
}

AssetNameDB::AssetNameDB()
{
    pathAsset = GetDataDir() / "assets.dat";
}

bool AssetNameDB::Write(const AssetName& data)
{
    return SerializeFileDB("assets", pathAsset, data);
}

bool AssetNameDB::Read(AssetName& data)
{
    return DeserializeFileDB(pathAsset, data);
}

}   // asset_db
