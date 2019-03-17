// TODO NOKUBIT: header


#include <nokubit/tagscript.h>
#include <base58.h>
#include <chain.h>
#include <chainparams.h>
#include <txmempool.h>
#include <validation.h>
#include <version.h>
#include <streams.h>
#include <hash.h>
#include <pubkey.h>
#include <key.h>
#include <univalue.h>

// TODO NOKUBIT: Per log
#include <core_io.h>
#include <util.h>
#include <utilstrencodings.h>
#include <utilmoneystr.h>

namespace asset_tag {

AssetTag *ScriptTag::CAssetTagMaker() const
{
    return new AssetTag(tagType, this);
}

ScriptTag* ScriptTag::ExtractTag(const NkbTagIdentifier& tag, const CScript &script)
{
    CScript::const_iterator pc = script.begin();
    std::vector<unsigned char> data;
    opcodetype opcode;
    if (!script.GetOp(pc, opcode, data) || data.size() < 4)
        return nullptr;
    if (!script.GetOp(pc, opcode) || opcode != OP_DROP)
        return nullptr;
    //if (!script.GetOp(pc, opcode) || opcode != OP_TRUE)
    //    return nullptr;
    if (pc != script.end())
        return nullptr;

    const std::string prefix((char *)data.data(), 3);
    unsigned char pegType = data[3];
    if (prefix != NKB_TAG_PREFIX || pegType != tag)
        return nullptr;

    data.erase(data.begin(), data.begin() + 4);
    CDataStream ssTag(data, SER_NETWORK, PROTOCOL_VERSION);
    switch (pegType)
    {
        case NkbPegin:
            {
            PeginScript *ps = new PeginScript();
            try {
                ssTag >> *ps;
                return (ScriptTag*)ps;
            }
            catch (const std::exception&) { }
            }
            break;
        case NkbOrganization:
            {
            OrganizationScript *os = new OrganizationScript();
            try {
                ssTag >> *os;
                return (ScriptTag*)os;
            }
            catch (const std::exception&) { }
            }
            break;
        case NkbMintAsset:
            {
            MintAssetScript *mas = new MintAssetScript();
            try {
                ssTag >> *mas;
                return (ScriptTag*)mas;
            }
            catch (const std::exception&) { }
            }
            break;
        // TODO NOKUBIT:
    }
    LogPrint(BCLog::ASSET, "\tScriptTag::ExtractTag BUILD FAILED for: '%c' %s\n", tag, HexStr(script));
    return nullptr;
}

ScriptTag* ScriptTag::ExtractTag(const NkbTagIdentifier& tag, const CScriptWitness &scriptWitness)
{
    if (scriptWitness.stack.size() == 2 && scriptWitness.stack[0].size() > 4) {
        std::vector<unsigned char> vch = scriptWitness.stack[0];
        const std::string prefix((char *)vch.data(), 3);
        unsigned char pegType = vch[3];
        if (prefix == NKB_TAG_PREFIX && pegType == tag)
        {
            vch.erase(vch.begin(), vch.begin() + 4);
            CDataStream ssTag(vch, SER_NETWORK, PROTOCOL_VERSION);
            switch (pegType)
            {
                case NkbPegin:
                    {
                    PeginScript *ps = new PeginScript();
                    try {
                        ssTag >> *ps;
                        return (ScriptTag*)ps;
                    }
                    catch (const std::exception&) { }
                    }
                    break;
                case NkbOrganization:
                    {
                    OrganizationScript *os = new OrganizationScript();
                    try {
                        ssTag >> *os;
                        return (ScriptTag*)os;
                    }
                    catch (const std::exception&) { }
                    }
                    break;
                case NkbMintAsset:
                    {
                    MintAssetScript *mas = new MintAssetScript();
                    try {
                        ssTag >> *mas;
                        return (ScriptTag*)mas;
                    }
                    catch (const std::exception&) { }
                    }
                    break;
                // TODO NOKUBIT:
            }
        }
        LogPrint(BCLog::ASSET, "\tScriptTag::ExtractTag INVALID pfx: %s\n", HexStr(vch.begin(), vch.begin() + 4));
    }
    LogPrint(BCLog::ASSET, "\tScriptTag::ExtractTag FAILED for: '%c' %s\n", tag, scriptWitness.ToString());
    return new ScriptTag();
}

// TODO una sola funzione non chiama il serializzatore giusto
const std::vector<unsigned char> ScriptTag::CreateTag() const
{
    if (!proof.empty()) {
        return proof;
    }
    ScriptTag::SetNull(false);

    std::vector<unsigned char> buffer(NKB_TAG_PREFIX.begin(), NKB_TAG_PREFIX.end());
    buffer.push_back(tagType);
    return buffer;
}

const Coin ScriptTag::GetTagCoin(int height) const
{
    CreateTag();
    return Coin(CTxOut(0, CScript() << proof << OP_DROP), height, CoinTypeTag);
}

const std::vector<unsigned char> ReferenceScript::CreateTag() const
{
    if (!proof.empty()) {
        return proof;
    }
    proof = ScriptTag::CreateTag();

    CVectorWriter stream(SER_NETWORK, PROTOCOL_VERSION, proof, proof.size());
    stream << *this;
    return proof;
}

const COutPoint ReferenceScript::GetTagOutpoint() const {
    if (outpoint.IsNull()) {
        uint32_t n = getTagType() | 0x80000000;
        const std::string dummy("ReferenceScript");
        std::vector<unsigned char> hash(dummy.begin(), dummy.end());
        hash.resize(32, 0);
        outpoint = COutPoint(uint256(hash), n);
    }
    return outpoint;
}

const std::vector<unsigned char> PeginScript::CreateTag() const
{
    if (!proof.empty()) {
        return proof;
    }
    proof = ScriptTag::CreateTag();

    CVectorWriter stream(SER_NETWORK, PROTOCOL_VERSION, proof, proof.size());
    stream << *this;
    return proof;
}

const COutPoint PeginScript::GetTagOutpoint() const {
    if (outpoint.IsNull()) {
        uint32_t n = getTagType() | 0x80000000;
        std::vector<unsigned char> vch;
        CVectorWriter stream(SER_NETWORK, PROTOCOL_VERSION, vch, 0);
        stream << chainGenesis;
        stream << blockHeight;
        stream << chainTx;
        outpoint = COutPoint(Hash(vch.cbegin(), vch.cend()), n);
    }
    return outpoint;
}

const std::vector<unsigned char> OrganizationScript::CreateTag() const
{
    if (!proof.empty()) {
        return proof;
    }
    proof = ScriptTag::CreateTag();

    CVectorWriter stream(SER_NETWORK, PROTOCOL_VERSION, proof, proof.size());
    stream << *this;
    return proof;
}

const COutPoint OrganizationScript::GetTagOutpoint() const {
    if (outpoint.IsNull()) {
        uint32_t n = getTagType() | 0x80000000;
        std::vector<unsigned char> hash(organizationName.cbegin(), organizationName.cend());
        hash.resize(32, 0);
        outpoint = COutPoint(uint256(hash), n);
    }
    return outpoint;
}

const std::vector<unsigned char> MintAssetScript::CreateTag() const
{
    if (!proof.empty()) {
        return proof;
    }
    proof = ScriptTag::CreateTag();

    CVectorWriter stream(SER_NETWORK, PROTOCOL_VERSION, proof, proof.size());
    stream << *this;
    return proof;
}

const COutPoint MintAssetScript::GetTagOutpoint() const {
    if (outpoint.IsNull()) {
        uint32_t n = getTagType() | 0x80000000;
        std::vector<unsigned char> hash(assetName.cbegin(), assetName.cend());
        hash.resize(32, 0);
        outpoint = COutPoint(uint256(hash), n);
    }
    return outpoint;
}

COutPoint ScriptTag::GetTagOutpoint(const NkbTagIdentifier& tagType, const std::string& name) {
    uint32_t n = tagType | 0x80000000;
    std::vector<unsigned char> hash(name.cbegin(), name.cend());
    uint256 h;
    if (hash.size() > 32) {
        h = Hash(hash.cbegin(), hash.cend());
    } else {
        hash.resize(32, 0);
        h = uint256(hash);
    }
    return COutPoint(h, n);
}

const CScript ScriptTag::GetWitness() const
{
    if (!witness.empty()) {
        return witness;
    }
    if (proof.empty()) {
        CreateTag();
    }
    uint160 hash = Hash160(proof);
    witness = CScript() << OP_HASH160 << ToByteVector(hash) << OP_EQUAL;
    return witness;
}

const CScript ScriptTag::GetWitnessProgram() const
{
    if (!witnessProgram.empty()) {
        return witnessProgram;
    }
    if (witness.empty()) {
        GetWitness();
    }
    uint256 hashWitness;
    CSHA256().Write(witness.data(), witness.size()).Finalize(hashWitness.begin());
    witnessProgram = CScript() << OP_0 << ToByteVector(hashWitness);
    return witnessProgram;
}

bool PeginScript::ValidateSign() const
{
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << *this;

    uint256 hash = ss.GetHash();
    const CPubKey& pubkey = Params().GetConsensus().pkNokubit;
    if (!pubkey.Verify(hash, signature))
    {
        LogPrint(BCLog::ASSET, "\tPegin ValidateSign: FAILED\n");
        tagValid = -1;
        return false;
    }
    tagValid = 1;
    return true;
}

bool OrganizationScript::ValidateSign() const
{
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << *this;

    uint256 hash = ss.GetHash();
    const CPubKey& pubkey = Params().GetConsensus().pkNokubit;
    if (!pubkey.Verify(hash, signature))
    {
        LogPrint(BCLog::ASSET, "\tOrganization ValidateSign: FAILED\n");
        tagValid = -1;
        return false;
    }
    tagValid = 1;
    return true;
}

bool MintAssetScript::ValidateSign() const
{
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << *this;

    uint256 hash = ss.GetHash();
    if (!assetAddress.Verify(hash, signature))
    {
        LogPrint(BCLog::ASSET, "\tAsset ValidateSign: FAILED\n");
        tagValid = -1;
        return false;
    }
    tagValid = 1;
    return true;
}

bool MintAssetScript::MakeSign(const CKey& key)
{
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << *this;

    uint256 hash = ss.GetHash();
    if (!key.Sign(hash, signature))
    {
        LogPrint(BCLog::ASSET, "\tAsset MakeSign: FAILED\n");
        return false;
    }
    return true;
}

static bool TagEnforceCoin(const CMutableTransaction& txPrev)
{
    bool inserted = false;
    const uint256 hashTx = txPrev.GetHash();

    {
    LOCK(cs_main);
    CCoinsViewCache &view = *pcoinsTip;
    // TODO rivedere coin assente
    Coin existingCoin = Coin(CTxOut(-4, CScript()), 1, CoinTypeCoin);
    //bool b = view.GetCoin(COutPoint(hashTx, 0), existingCoin);
    view.GetCoin(COutPoint(hashTx, 0), existingCoin);
    if (existingCoin.out.nValue == -4) {
        view.AddCoin(COutPoint(hashTx, 0), Coin(txPrev.vout[0], MEMPOOL_HEIGHT, CoinTypeCoin), true, true);
        inserted = true;
    }

    } // cs_main

    return inserted;
}

bool PeginScript::EnforceTag(uint256 *hash) const
{
    CMutableTransaction txPrev;
    ReferenceScript(chainTx, GetWitnessProgram(), GetAmount()).MakeTagTransaction(txPrev);
    if (hash != nullptr) {
        *hash = txPrev.GetHash();
    }
    return TagEnforceCoin(txPrev);
}

bool OrganizationScript::EnforceTag(uint256 *hash) const
{
    CMutableTransaction txPrev;
    ReferenceScript(COutPoint(uint256S(organizationAddress.ToString()), enableFlag), GetWitnessProgram(), GetAmount()).MakeTagTransaction(txPrev);
    if (hash != nullptr) {
        *hash = txPrev.GetHash();
    }
    return TagEnforceCoin(txPrev);
}

bool OrganizationScript::IsReplaceable(const Coin& coin) const
{
    if (coin.IsSpent() || !coin.IsTag())
        return false;
    const OrganizationScript *tag = static_cast<const OrganizationScript*>(ExtractTag(NkbOrganization, coin.out.scriptPubKey));
    if (tag == nullptr || organizationName != tag->organizationName || organizationAddress != tag->organizationAddress)
        return false;
    return true;
}

bool MintAssetScript::EnforceTag(uint256 *hash) const
{
    CMutableTransaction txPrev;
    ReferenceScript(COutPoint(uint256S(address.ToString()), 0), GetWitnessProgram(), GetAmount()).MakeTagTransaction(txPrev);
    if (hash != nullptr) {
        *hash = txPrev.GetHash();
    }
    return TagEnforceCoin(txPrev);
}

bool MintAssetScript::IsReplaceable(const Coin& coin) const
{
    if (coin.IsSpent() || !coin.IsTag())
        return false;
    const MintAssetScript *tag = static_cast<const MintAssetScript*>(ExtractTag(NkbMintAsset, coin.out.scriptPubKey));
    if (tag == nullptr || assetName != tag->assetName || assetAddress != tag->assetAddress)
        return false;
    if (!tag->issuanceFlag && nAssetValue != 0)
        return false;
    return true;
}

bool ReferenceScript::MakeTagTransaction(CMutableTransaction& txOut) const
{
    txOut.vin.resize(1);
    txOut.vout.resize(1);
    txOut.vin[0] = CTxIn(refTx);
    txOut.vin[0].SetTag(ScriptTag::CAssetTagMaker());

    CScript witnessProgScript = GetWitness();

    CScriptWitness tag_witness;
    std::vector<std::vector<unsigned char> >& stack = tag_witness.stack;
    stack.resize(2);
    stack[0] = CreateTag();
    stack[1] = ToByteVector(witnessProgScript);
    txOut.vin[0].scriptWitness = tag_witness;

    txOut.vout[0] = CTxOut(nValue, parentWitnessProgram);

    return false;
}

bool ReferenceScript::CheckTag(const CTransaction& tx, std::string &error) const
{
    // ERRORE SEMPRE?
    if (tx.vin.size() != 1) {
        error = "invalid reference transaction";
        return false;
    }
    const CTxIn txin = tx.vin[0];
    if (!txin.IsTagged()) {
        error = "empty reference transaction";
        return false;
    }
    if (txin.scriptWitness.IsNull() || txin.scriptWitness.stack.size() < 2) {
        error = "empty reference witness";
        return false;
    }
    return true;
}

bool PeginScript::MakeTagTransaction(CMutableTransaction& txOut) const
{
    uint256 hashTx;
    bool inserted = EnforceTag(&hashTx);

    // One peg-in input, one wallet output (and one fee output)
    txOut.vin.resize(1);
    txOut.vout.resize(1);
    txOut.vin[0] = CTxIn(COutPoint(hashTx, 0));
    // set peg-in input
    txOut.vin[0].SetTag(ScriptTag::CAssetTagMaker());

    CScript witnessProgScript = GetWitness();

    // Construct pegin proof
    CScriptWitness pegin_witness;
    std::vector<std::vector<unsigned char> >& stack = pegin_witness.stack;
    stack.resize(2);
    stack[0] = CreateTag();
    stack[1] = ToByteVector(witnessProgScript);
    txOut.vin[0].scriptWitness = pegin_witness;

    CScript scriptPubKey = GetScriptForDestination(destinationAddress);
    txOut.vout[0] = CTxOut(nValue, scriptPubKey);

    return inserted;
}

bool PeginScript::CheckTag(const CTransaction& tx, std::string &error) const
{
    if (tx.vin.size() != 1 || tx.vout.size() != 1) {
        error = "invalid pegin transaction";
        return false;
    }

    const CTxIn txin = tx.vin[0];
    if (!txin.IsTagged()) {
        error = "empty pegin transaction";
        return false;
    }
    // Quick check of tag witness
    if (txin.scriptWitness.IsNull() || txin.scriptWitness.stack.size() < 2) {
        error = "empty pegin witness";
        return false;
    }

    if (tx.vout[0].nValue.GetAmountName() != COIN_ASSET) {
        error = "malleability pegin vout";
        return false;
    }
    CScript scriptPubKey = GetScriptForDestination(destinationAddress);
    if (tx.vout[0].scriptPubKey != scriptPubKey) {
        error = "invalid pegin vout";
        return false;
    }
    return true;
}

bool OrganizationScript::MakeTagTransaction(CMutableTransaction& txOut) const
{
    uint256 hashTx;
    bool inserted = EnforceTag(&hashTx);

// TODO     spostato in UpdateCoins
// serve la ricerca nel DB ed eventuale validazione
// qui o meglio nel consensus

    // One state input, one data output, (one wallet output and one fee output)
    txOut.vin.resize(1);
    txOut.vout.resize(1);
    txOut.vin[0] = CTxIn(COutPoint(hashTx, 0));
    // set state input
    txOut.vin[0].SetTag(ScriptTag::CAssetTagMaker());

    CScript witnessProgScript = GetWitness();
    // Construct state proof
    CScriptWitness state_witness;
    std::vector<std::vector<unsigned char> >& stack = state_witness.stack;
    stack.resize(2);
    stack[0] = CreateTag();
    stack[1] = ToByteVector(witnessProgScript);
    txOut.vin[0].scriptWitness = state_witness;

    std::vector<unsigned char> data = ToByteVector(organizationName + " " + (enableFlag ? "ON" : "OFF"));
    CScript stateScript = CScript() << OP_RETURN << data;
    txOut.vout[0] = CTxOut(0, stateScript);

    return inserted;
}

bool OrganizationScript::CheckTag(const CTransaction& tx, std::string &error) const
{
    if (tx.vin.size() < 1) {
        error = "invalid organization transaction";
        return false;
    }
    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        const CTxIn txin = tx.vin[i];
        if (i == 0) {
            if (!txin.IsTagged()) {
                error = "empty organization transaction";
                return false;
            }
            if (txin.scriptWitness.IsNull() || txin.scriptWitness.stack.size() < 2) {
                error = "empty organization witness";
                return false;
            }
        } else {
            if (txin.IsTagged()) {
                error = "invalid organization transaction";
                return false;
            }
        }
    }

    for (unsigned int i = 0; i < tx.vout.size(); i++) {
        const CTxOut txout = tx.vout[i];
        if (i == 0) {
            if (!txout.scriptPubKey.IsUnspendable()) {
                error = "invalid organization vout";
                return false;
            }
        }
        if (txout.nValue.GetAmountName() != COIN_ASSET) {
            error = "invalid organization vout";
            return false;
        }
    }
    return true;
}

bool MintAssetScript::MakeTagTransaction(CMutableTransaction& txOut) const
{
    uint256 hashTx;
    bool inserted = EnforceTag(&hashTx);

// TODO     spostato in UpdateCoins
// serve la ricerca nel DB ed eventuale validazione
// qui o meglio nel consensus

    // One asset input, one wallet output and one fee output
    txOut.vin.resize(1);
    txOut.vout.resize(1);
    txOut.vin[0] = CTxIn(COutPoint(hashTx, 0));
    // set asset input
    txOut.vin[0].SetTag(ScriptTag::CAssetTagMaker());

    CScript witnessProgScript = GetWitness();

    // Construct state proof
    CScriptWitness state_witness;
    std::vector<std::vector<unsigned char> >& stack = state_witness.stack;
    stack.resize(2);
    stack[0] = CreateTag();
    stack[1] = ToByteVector(witnessProgScript);
    txOut.vin[0].scriptWitness = state_witness;

    CScript scriptPubKey = GetScriptForDestination(destinationAddress);
    txOut.vout[0] = CTxOut(AssetAmount(assetName, nAssetValue), scriptPubKey);

    return inserted;
}

bool MintAssetScript::CheckTag(const CTransaction& tx, std::string &error) const
{
    if (tx.vin.size() < 1) {
        error = "invalid mintasset transaction";
        return false;
    }
    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        const CTxIn txin = tx.vin[i];
        if (i == 0) {
            if (!txin.IsTagged()) {
                error = "empty mintasset transaction";
                return false;
            }
            if (txin.scriptWitness.IsNull() || txin.scriptWitness.stack.size() < 2) {
                error = "empty mintasset witness";
                return false;
            }
        } else {
            if (txin.IsTagged()) {
                error = "invalid mintasset transaction";
                return false;
            }
        }
    }

    for (unsigned int i = 0; i < tx.vout.size(); i++) {
        const CTxOut txout = tx.vout[i];
        if (i == 0) {
            if (txout.nValue.GetAmountName() != assetName) {
                error = "spend different asset";
                return false;
            }
            CScript scriptPubKey = GetScriptForDestination(destinationAddress);
            if (txout.scriptPubKey != scriptPubKey) {
                error = "malleability mintasset vout";
                return false;
            }
        } else {
            if (txout.nValue.GetAmountName() != COIN_ASSET) {
                error = "invalid mintasset vout";
                return false;
            }
        }
    }
    return true;
}

void ScriptTag::ToUniv(UniValue& uv) const
{
    const std::string type(1, tagType);
    uv.pushKV("type", NKB_TAG_PREFIX + type);
    if (tagValid != 0)
        uv.pushKV("valid", (tagValid > 0));
}

std::string ScriptTag::ToString() const
{
    std::string str;
    str += "ScriptTag(";
    if (tagValid == 0)
        str += "Not validated";
    else if (tagValid > 0)
        str += "Validated=TRUE";
    else
        str += "Validated=FALSE";
    str += ")";
    return str;
}

void ReferenceScript::ToUniv(UniValue& uv) const
{
    uv.pushKV("type", "ReferenceScript");
    uv.pushKV("reference_txid", refTx.hash.ToString());
    uv.pushKV("reference_txvout", (int64_t)refTx.n);
    uv.pushKV("reference", HexStr(parentWitnessProgram.begin(), parentWitnessProgram.end()));
    uv.pushKV("amount", ValueFromAmount(nValue));
}

std::string ReferenceScript::ToString() const
{
    std::string str;
    str += "ReferenceScript(";
    str += strprintf("Ref Tx=%s : %u", refTx.hash.ToString(), refTx.n);
    str += strprintf(", ParentWitnessProgram: %s", FormatScript(parentWitnessProgram));
    str += strprintf(", Amount: %s", FormatMoney(nValue));
    str += ")";
    return str;
}

void PeginScript::ToUniv(UniValue& uv) const
{
    uv.pushKV("type", "PeginScript");
    if (chainGenesis == uint256S("0x000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f"))
        uv.pushKV("chainGenesis", "bitcoin");
    else
        uv.pushKV("chainGenesis", chainGenesis.ToString());
    uv.pushKV("blockHeight", (int64_t)blockHeight);
    uv.pushKV("txid", chainTx.hash.ToString());
    uv.pushKV("txvout", (int64_t)chainTx.n);
    uv.pushKV("destination", EncodeDestination(destinationAddress));
    uv.pushKV("amount", ValueFromAmount(nValue));
    //uv.pushKV("signature", HexStr(signature.begin(), signature.end()));
    if (tagValid != 0)
        uv.pushKV("valid", (tagValid > 0));
}

std::string PeginScript::ToString() const
{
    std::string str;
    str += "PeginScript(";
    str += strprintf("From Chain=%s", (chainGenesis == uint256S("0x000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f") ? "bitcoin" : chainGenesis.ToString()));
    str += strprintf(", Block=%u", blockHeight);
    str += strprintf(", TxOut=%s : %u", chainTx.hash.ToString(), chainTx.n);
    str += strprintf(", Destination=%s", EncodeDestination(destinationAddress));
    str += strprintf(", Amount: %s", FormatMoney(nValue));
    if (tagValid == 0)
        str += ", Not validated";
    else if (tagValid > 0)
        str += ", Validated=TRUE";
    else
        str += ", Validated=FALSE";
    str += ")";
    return str;
}

void OrganizationScript::ToUniv(UniValue& uv) const
{
    uv.pushKV("type", "OrganizationScript");
    uv.pushKV("name", organizationName);
    uv.pushKV("description", organizationDescription);
    uv.pushKV("address", EncodeDestination(organizationAddress));
    //uv.pushKV("signature", HexStr(signature.begin(), signature.end()));
    uv.pushKV("enabled", enableFlag);
    if (tagValid != 0)
        uv.pushKV("valid", (tagValid > 0));
}

std::string OrganizationScript::ToString() const
{
    std::string str;
    str += "OrganizationScript(";
    str += strprintf("Name=%s", organizationName);
    str += strprintf(", Description=%s", organizationDescription);
    str += strprintf(", Address=%s", EncodeDestination(organizationAddress));
    if (enableFlag)
        str += ", Enabled";
    else
        str += ", Disabled";
    if (tagValid == 0)
        str += ", Not validated";
    else if (tagValid > 0)
        str += ", Validated=TRUE";
    else
        str += ", Validated=FALSE";
    str += ")";
    return str;
}

void MintAssetScript::ToUniv(UniValue& uv) const
{
    uv.pushKV("type", "MintAssetScript");
    uv.pushKV("name", assetName);
    uv.pushKV("description", assetDescription);
    uv.pushKV("address", HexStr(assetAddress.begin(), assetAddress.end()));
    if (!stateAddress.IsNull())
        uv.pushKV("state", EncodeDestination(stateAddress));
    uv.pushKV("destination", EncodeDestination(destinationAddress));
    uv.pushKV("issuance", (issuanceFlag ? "open" : "closed"));
    //extraWitness
    //uv.pushKV("signature", HexStr(signature.begin(), signature.end()));
    uv.pushKV("amount", nAssetValue);
    if (tagValid != 0)
        uv.pushKV("valid", (tagValid > 0));
}

std::string MintAssetScript::ToString() const
{
    std::string str;
    str += "MintAssetScript(";
    str += strprintf("Name=%s", assetName);
    str += strprintf(", Description=%s", assetDescription);
    str += strprintf(", PubKey=%s", HexStr(assetAddress.begin(), assetAddress.end()));
    if (!stateAddress.IsNull())
        str += strprintf(", State=%s", stateAddress.ToString());
    str += strprintf(", Destination=%s", EncodeDestination(destinationAddress));
    if (issuanceFlag)
        str += ", Open to reissuance";
    else
        str += ", Closed to reissuance";
    str += strprintf(", amount: %ld", nAssetValue);
    // str += strprintf(", amount: %s", FormatMoney(nValue));
    // str += strprintf(", fee: %s", FormatMoney(nFee));
    if (tagValid == 0)
        str += ", Not validated";
    else if (tagValid > 0)
        str += ", Validated=TRUE";
    else
        str += ", Validated=FALSE";
    str += ")";
    return str;
}


PoSScript* PoSScript::ExtractTag(const CScriptWitness &scriptWitness, const uint256& hash)
{
    if (scriptWitness.stack.size() == 2 && scriptWitness.stack[0].size() > 4) {
        std::vector<unsigned char> vch = scriptWitness.stack[0];
        const std::string prefix((char *)vch.data(), 3);
        if (prefix == NKB_TAG_PREFIX && NkbPoS == vch[3])
        {
            vch.erase(vch.begin(), vch.begin() + 4);
            CScript sign = CScript() << scriptWitness.stack[1];
            vch.insert(vch.end(), sign.begin(), sign.end());
            CDataStream ssTag(vch, SER_NETWORK, PROTOCOL_VERSION);
            try {
                PoSScript *ps = new PoSScript();
                ssTag >> *ps;
                ps->blockHash = hash;
                return ps;
            }
            catch (const std::exception&) { }
        }
        LogPrint(BCLog::ASSET, "\tPoSScript::ExtractTag INVALID pfx: %s\n", HexStr(vch.begin(), vch.begin() + 4));
    }
    LogPrint(BCLog::ASSET, "\tPoSScript::ExtractTag FAILED for: %s\n", scriptWitness.ToString());
    return nullptr;
}

bool PoSScript::ValidateSign() const
{
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << *this;

    uint256 hash = ss.GetHash();
    if (!stackAddress.Verify(hash, signature))
    {
        LogPrint(BCLog::ASSET, "\tPos ValidateSign: FAILED\n");
        posValid = -1;
        return false;
    }
    CAmount totStack = 100000 * COIN;
    if (Params().GetConsensus().pkPoANoku.find(address) == Params().GetConsensus().pkPoANoku.end()) {
        // TODO NOKUBIT
        //          per POS nokubit verificare e spostare stack
        //          recuperare totStack
        posValid = -1;
        return false;
    }
    double maxStack = (double)totStack / 1000;
    double curStack = nStackValue;
    if (curStack > maxStack)
        curStack = maxStack;
    int curExp = 0;
    double curFrac = frexp(curStack, &curExp);
    // TODO NOKUBIT
    //          mettere un controllo sul minimo numero di bit 0
    if (curExp < 32) {
        curFrac *= 1 << curExp;
        curExp = 0;
    } else {
        curFrac *= 1LL << 32;
        curExp -= 32;
    }
    arith_uint256 aBlockHash = UintToArith256(blockHash);
LogPrint(BCLog::ASSET, "\tPoSScript::ValidateSign: Hash %s\n", aBlockHash.ToString());
    arith_uint256 aCurFrac = arith_uint256(curFrac);
    aBlockHash /= aCurFrac;
    aBlockHash >>= curExp;
    //dPosHash = sqrt(dBlockHash * dBlockHash + dPosHash * dPosHash);
LogPrint(BCLog::ASSET, "\tPoSScript::ValidateSign: PosHash %s\n", aBlockHash.ToString());
    posHash = ArithToUint256(aBlockHash);
    posValid = 1;
    return true;
}

const uint256 PoSScript::GetPoSHash() const
{
    if (IsValidate())
        return posHash;
    return blockHash;
}

void PoSScript::ToUniv(UniValue& uv) const
{
    uv.pushKV("type", "PoSScript");
    uv.pushKV("txid", stack.hash.ToString());
    uv.pushKV("txvout", (int64_t)stack.n);
    uv.pushKV("amount", ValueFromAmount(nStackValue));
    uv.pushKV("signer", HexStr(stackAddress.begin(), stackAddress.end()));
    uv.pushKV("hash", blockHash.ToString());
    uv.pushKV("hashpos", posHash.ToString());
    //uv.pushKV("signature", HexStr(signature.begin(), signature.end()));
    if (posValid != 0)
        uv.pushKV("valid", (posValid > 0));
}

std::string PoSScript::ToString() const
{
    std::string str;
    str += "PoSScript(";
    str += strprintf("Stack Tx=%s : %u", stack.hash.ToString(), stack.n);
    str += strprintf(", Amount: %s", FormatMoney(nStackValue));
    str += strprintf(", Signer=%s", HexStr(stackAddress.begin(), stackAddress.end()));
    str += strprintf(", Hash=%s", blockHash.ToString());
    str += strprintf(", HashPos=%s", posHash.ToString());
    if (posValid == 0)
        str += ", Not validated";
    else if (posValid > 0)
        str += ", Validated=TRUE";
    else
        str += ", Validated=FALSE";
    str += ")";
    return str;
}

}   // asset_tag
