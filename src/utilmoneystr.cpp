// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <utilmoneystr.h>

#include <primitives/transaction.h>
#include <tinyformat.h>
#include <utilstrencodings.h>

// NOKUBIT: Nokubit AssetCoin
#include <boost/algorithm/string.hpp>

std::string FormatMoney(const CAmount& n)
{
    // Note: not using straight sprintf here because we do NOT want
    // localized number formatting.
    int64_t n_abs = (n > 0 ? n : -n);
    int64_t quotient = n_abs/COIN;
    int64_t remainder = n_abs%COIN;
    std::string str = strprintf("%d.%08d", quotient, remainder);

    // Right-trim excess zeros before the decimal point:
    int nTrim = 0;
    for (int i = str.size()-1; (str[i] == '0' && isdigit(str[i-2])); --i)
        ++nTrim;
    if (nTrim)
        str.erase(str.size()-nTrim, nTrim);

    if (n < 0)
        str.insert((unsigned int)0, 1, '-');
    return str;
}

// NOKUBIT: Nokubit AssetCoin
std::string FormatMoney(const asset_tag::AssetAmount& n)
{
    if (n != asset_tag::COIN_ASSET) {
        return strprintf("%d %s", n.GetAmountValue(), n.GetAmountName());
    }
    return FormatMoney(n.GetAmountValue());
}

// NOKUBIT: Nokubit AssetCoin
std::string FormatMoney(const asset_tag::CAmountMap& nRef)
{
    bool flag = false;
    std::string str = "";
    for (asset_tag::CAmountMap::const_iterator it = nRef.begin(); it != nRef.end(); it++) {
        if (flag) str += ", ";
        flag = true;
        str += FormatMoney(asset_tag::AssetAmount(*it));
    }
    return str;
}


bool ParseMoney(const std::string& str, CAmount& nRet)
{
    return ParseMoney(str.c_str(), nRet);
}

bool ParseMoney(const char* pszIn, CAmount& nRet)
{
    std::string strWhole;
    int64_t nUnits = 0;
    const char* p = pszIn;
    while (isspace(*p))
        p++;
    for (; *p; p++)
    {
        if (*p == '.')
        {
            p++;
            int64_t nMult = CENT*10;
            while (isdigit(*p) && (nMult > 0))
            {
                nUnits += nMult * (*p++ - '0');
                nMult /= 10;
            }
            break;
        }
        if (isspace(*p))
            break;
        if (!isdigit(*p))
            return false;
        strWhole.insert(strWhole.end(), *p);
    }
    for (; *p; p++)
        if (!isspace(*p))
            return false;
    if (strWhole.size() > 10) // guard against 63 bit overflow
        return false;
    if (nUnits < 0 || nUnits > COIN)
        return false;
    int64_t nWhole = atoi64(strWhole);
    CAmount nValue = nWhole*COIN + nUnits;

    nRet = nValue;
    return true;
}

// NOKUBIT: Nokubit AssetCoin
bool ParseMoney(const std::string& str, asset_tag::AssetAmount& nRet)
{
    asset_tag::AssetName name(asset_tag::COIN_ASSET);
    std::vector<std::string> vStrInputParts;
    boost::split(vStrInputParts, str, boost::is_any_of(" "));

    if (vStrInputParts.size() > 2)
        return false;
    if (vStrInputParts.size() == 2) {
        std::string strAsset;
        const char* p = vStrInputParts[1].c_str();
        while (isspace(*p))
            p++;
        if (!isdigit(*p))
            return false;
        for (; *p; p++)
        {
            if (isspace(*p))
                break;
            strAsset.insert(strAsset.end(), *p);
        }
        for (; *p; p++)
            if (!isspace(*p))
                return false;
        if (strAsset.size() < asset_tag::ASSET_NAME_MIN_SIZE || strAsset.size() > asset_tag::ASSET_NAME_MAX_SIZE)
            return false;
        name = strAsset;
    }

    CAmount nValue;
    if (!ParseMoney(vStrInputParts[0].c_str(), nValue))
        return false;

    nRet = asset_tag::AssetAmount(name, nValue);
    return true;
}
