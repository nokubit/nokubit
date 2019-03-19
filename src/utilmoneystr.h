// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * Money parsing/formatting utilities.
 */
#ifndef BITCOIN_UTILMONEYSTR_H
#define BITCOIN_UTILMONEYSTR_H

#include <stdint.h>
#include <string>

#include <amount.h>
// NOKUBIT: Nokubit AssetCoin
#include <nokubit/asset.h>

/* Do not use these functions to represent or parse monetary amounts to or from
 * JSON but use AmountFromValue and ValueFromAmount for that.
 */
std::string FormatMoney(const CAmount& n);
// NOKUBIT: Nokubit AssetCoin
std::string FormatMoney(const asset_tag::AssetAmount& n);
std::string FormatMoney(const asset_tag::CAmountMap& nRef);
bool ParseMoney(const std::string& str, CAmount& nRet);
bool ParseMoney(const char* pszIn, CAmount& nRet);
// NOKUBIT: Nokubit AssetCoin
bool ParseMoney(const std::string& str, asset_tag::AssetAmount& nRet);

#endif // BITCOIN_UTILMONEYSTR_H
