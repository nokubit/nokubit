// TODO NOKUBIT: header


#include <nokubit/asset.h>
#include <nokubit/tagscript.h>
#include <chainparams.h>
#include <version.h>
#include <streams.h>
#include <hash.h>
#include <pubkey.h>
#include <univalue.h>


namespace asset_tag {

AssetTag::~AssetTag()
{
    delete tag_ptr;
}

inline void AssetTag::extract() const
{
    if (tag_ptr == nullptr) {
        tag_ptr = ScriptTag::ExtractTag(tagType, scriptWitness);
    }
}

bool AssetTag::CheckTag(const CTransaction& tx, std::string &error) const
{
    extract();
    return tag_ptr->CheckTag(tx, error);
}

bool AssetTag::EnforceTag() const
{
    extract();
    return tag_ptr->EnforceTag();
}

void AssetTag::GetTagOutpoint(COutPoint& out) const
{
    extract();
    out = tag_ptr->GetTagOutpoint();
}

void AssetTag::GetTagCoin(Coin& coin, int height) const
{
    extract();
    coin = tag_ptr->GetTagCoin(height);
}

bool AssetTag::IsValidate() const
{
    extract();
    return tag_ptr->IsValidate();
}

bool AssetTag::IsReplaceable(const Coin& coin) const
{
    extract();
    return tag_ptr->IsReplaceable(coin);
}

void AssetTag::ToUniv(UniValue& uv) const
{
    extract();
    return tag_ptr->ToUniv(uv);
}

bool AssetTag::operator==(AssetTag& o) const
{
    if (tagType != o.tagType)
        return false;
    extract();
    o.extract();
    if (tag_ptr != nullptr && o.tag_ptr != nullptr)
        return *tag_ptr == *o.tag_ptr;
    return false;
}

bool AssetTag::operator!=(AssetTag& o) const
{
    return !(*this == o);
}

AssetAmount AssetTag::GetAmount() const
{
    extract();
    return tag_ptr->GetAmount();
}

std::string AssetTag::ToString() const
{
    extract();
    return tag_ptr->ToString();
}

PoSTag::~PoSTag()
{
    delete pos_ptr;
}

inline bool PoSTag::extract() const
{
    if (pos_ptr == nullptr && !scriptWitness.IsNull()) {
        pos_ptr = PoSScript::ExtractTag(scriptWitness, headerHash);
    }
    return pos_ptr != nullptr;
}

const uint256 PoSTag::GetPoSHash() const
{
    if (extract())
        return pos_ptr->GetPoSHash();
    return headerHash;
}

bool PoSTag::IsValidate() const
{
    if (extract())
        return pos_ptr->IsValidate();
    return false;
}

// bool PoSTag::operator==(PoSTag& o) const
// {
//     extract();
//     o.extract();
//     if (pos_ptr != nullptr && o.pos_ptr != nullptr)
//         return *pos_ptr == *o.pos_ptr;
//     return headerHash == o.headerHash;
// }

// bool PoSTag::operator!=(PoSTag& o) const
// {
//     return !(*this == o);
// }

AssetAmount PoSTag::GetAmount() const
{
    if (extract())
        return pos_ptr->GetAmount();
    return AssetAmount(COIN_ASSET, 0);
}

void PoSTag::ToUniv(UniValue& uv) const
{
    if (extract())
        return pos_ptr->ToUniv(uv);
    uv.pushKV("type", "NoPoSScript");
    uv.pushKV("hash", headerHash.ToString());
}

std::string PoSTag::ToString() const
{
    if (extract())
        return pos_ptr->ToString();
    std::string str;
    str += "NoPoSScript(";
    str += strprintf("Hash=%s", headerHash.ToString());
    str += ")";
    return str;
}

}   // asset_tag
