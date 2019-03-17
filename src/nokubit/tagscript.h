// TODO NOKUBIT: header


#ifndef BITCOIN_NOKUBIT_TAGSCRIPT_H
#define BITCOIN_NOKUBIT_TAGSCRIPT_H

#include <amount.h>
#include <coins.h>
#include <primitives/transaction.h>
#include <script/standard.h>
#include <script/script.h>
#include <pubkey.h>
#include <key.h>
#include <serialize.h>
#include <uint256.h>
#include <nokubit/asset.h>
#include <vector>
#include <stdint.h>
#include <string>


namespace asset_tag {

class ScriptTag
{
private:
    const NkbTagIdentifier tagType;

protected:
    mutable int tagValid;
    mutable std::vector<unsigned char> proof;
    mutable COutPoint outpoint;
    mutable CScript witness;
    mutable CScript witnessProgram;

    ScriptTag() : tagType(NkbNone) { SetNull(); }
    ScriptTag(const NkbTagIdentifier type) : tagType(type) { SetNull(); }
    void SetNull(bool flag = true) const
    {
        if (flag) tagValid = 0;
        proof.clear();
        witness.clear();
        witnessProgram.clear();
    }
    AssetTag *CAssetTagMaker() const;
    static ScriptTag* ExtractTag(const NkbTagIdentifier& tag, const CScript &script);

public:
    virtual ~ScriptTag() { }

    virtual bool EnforceTag(uint256 *hash = nullptr) const { return false; }
    virtual bool IsValidate() const { return (tagValid > 0); }
    virtual bool IsReplaceable(const Coin& coin) const { return false; }
    NkbTagIdentifier getTagType() const { return tagType; }
    bool IsTagType(NkbTagIdentifier tag) const { return tagType == tag; }

    virtual bool operator==(const ScriptTag& o) const
    {
        if (tagType != o.tagType)
            return false;
        return true;
    }

    virtual bool operator!=(const ScriptTag& o) const
    {
        return !(*this == o);
    }

    virtual const std::vector<unsigned char> CreateTag() const;
    virtual const COutPoint GetTagOutpoint() const { return outpoint; };
    virtual const Coin GetTagCoin(int height) const;
    virtual const CScript GetWitness() const;
    virtual const CScript GetWitnessProgram() const;
    virtual bool MakeTagTransaction(CMutableTransaction& txOut) const { return false; }
    virtual bool CheckTag(const CTransaction& tx, std::string &error) const { return false; }
    virtual AssetAmount GetAmount() const { return AssetAmount(COIN_ASSET, -1); }
    virtual CAmount GetFee() const { return 0; }
    virtual void ToUniv(UniValue& uv) const;
    virtual std::string ToString() const;

    static ScriptTag* ExtractTag(const NkbTagIdentifier& tag, const CScriptWitness &scriptWitness);
    static COutPoint GetTagOutpoint(const NkbTagIdentifier& tagType, const std::string& name);
};

class ReferenceScript : public ScriptTag
{
public:
    COutPoint refTx;
    AssetAmount nValue;
    CScript parentWitnessProgram;

public:
    ReferenceScript() : ScriptTag(NkbReference)
    {
        SetNull();
    }

    ~ReferenceScript() { }

    ReferenceScript(const COutPoint& refTxIn, const CScript witnessProgram, const AssetAmount nValueIn)
    : ScriptTag(NkbReference)
    {
        refTx = refTxIn;
        parentWitnessProgram = witnessProgram;
        nValue = nValueIn;
    }

    bool IsValidate() const override
    {
        if (tagValid == 0) {
            tagValid = 1;
        }
        return (tagValid > 0);
    }

    bool operator==(const ScriptTag& o) const override
    {
        if (!o.IsTagType(NkbReference))
            return false;
        const ReferenceScript* op = static_cast<const ReferenceScript*>(&o);
        return (this->refTx == op->refTx &&
                this->parentWitnessProgram == op->parentWitnessProgram &&
                this->nValue  == op->nValue);
    }

    bool operator!=(const ScriptTag& o) const override
    {
        return !(*this == o);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(refTx);
        READWRITE(parentWitnessProgram);
        READWRITE(nValue);
    }

    void SetNull()
    {
        ScriptTag::SetNull();
        refTx.SetNull();
        parentWitnessProgram.clear();
        nValue = AssetAmount(COIN_ASSET, -1);
    }

    const std::vector<unsigned char> CreateTag() const override;
    const COutPoint GetTagOutpoint() const override;
    bool MakeTagTransaction(CMutableTransaction& txOut) const override;
    bool CheckTag(const CTransaction& tx, std::string &error) const override;
    AssetAmount GetAmount() const override
    {
        return nValue;
    }
    void ToUniv(UniValue& uv) const override;
    std::string ToString() const override;
};

class PeginScript : public ScriptTag
{
public:
    uint256 chainGenesis;                   // Pegged chain
    uint32_t blockHeight;                   // Pegged block
    COutPoint chainTx;                      // Pegged txout
    WitnessV0KeyHash destinationAddress;    // destinatario
    std::vector<unsigned char> signature;
    CAmount nValue;                         // Pegged value

public:
    PeginScript() : ScriptTag(NkbPegin)
    {
        SetNull();
    }

    ~PeginScript() { }

    PeginScript(
        const uint256& chainGenesisIn,
        const uint32_t blockHeightIn,
        const COutPoint& chainTxIn,
        const WitnessV0KeyHash& destinationAddressIn,
        const std::vector<unsigned char>& signatureIn,
        const CAmount nValueIn)
    : ScriptTag(NkbPegin)
    {
        chainGenesis = chainGenesisIn;
        blockHeight = blockHeightIn;
        chainTx = chainTxIn;
        destinationAddress = destinationAddressIn;
        signature = signatureIn;
        nValue = nValueIn;
    }

    bool EnforceTag(uint256 *hash = nullptr) const override;
    bool IsValidate() const override
    {
        if (tagValid == 0) {
            if (ValidateSign()) {
                return true;
            }
            return false;
        }
        return (tagValid > 0);
    }

    // la firma non comporta disuguaglianza
    bool operator==(const ScriptTag& o) const override
    {
        if (!o.IsTagType(NkbPegin))
            return false;
        const PeginScript* op = static_cast<const PeginScript*>(&o);
        return (this->chainGenesis       == op->chainGenesis &&
                this->blockHeight        == op->blockHeight &&
                this->chainTx            == op->chainTx &&
                this->destinationAddress == op->destinationAddress &&
                this->nValue             == op->nValue);
    }

    bool operator!=(const ScriptTag& o) const override
    {
        return !(*this == o);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(chainGenesis);
        READWRITE(blockHeight);
        READWRITE(chainTx);
        READWRITE(destinationAddress);
        READWRITE(nValue);
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(signature);
    }

    void SetNull()
    {
        ScriptTag::SetNull();
        chainGenesis.SetNull();
        blockHeight = (uint32_t) -1;
        chainTx.SetNull();
        destinationAddress.SetNull();
        signature.clear();
        nValue = -1;
    }

    bool ValidateSign() const;
    const std::vector<unsigned char> CreateTag() const override;
    const COutPoint GetTagOutpoint() const override;
    bool MakeTagTransaction(CMutableTransaction& txOut) const override;
    bool CheckTag(const CTransaction& tx, std::string &error) const override;
    AssetAmount GetAmount() const override
    {
        return AssetAmount(COIN_ASSET, nValue);
    }
    void ToUniv(UniValue& uv) const override;
    std::string ToString() const override;
};

class OrganizationScript : public ScriptTag
{
public:
    std::string organizationName;           // Nome univoco
    std::string organizationDescription;    // serve ?
    WitnessV0KeyHash organizationAddress;   // emittente
    unsigned char enableFlag;               // Flag: enable/disable
    std::vector<unsigned char> signature;

public:
    OrganizationScript() : ScriptTag(NkbOrganization)
    {
        SetNull();
    }

    ~OrganizationScript() { }

    OrganizationScript(
        const std::string& organizationNameIn,
        const WitnessV0KeyHash& organizationAddressIn,
        const std::vector<unsigned char>& signatureIn,
        const bool enableFlagIn = true,
        const std::string& organizationDescriptionIn = ""
    )
    : ScriptTag(NkbOrganization)
    {
        organizationName = organizationNameIn;
        organizationAddress = organizationAddressIn;
        signature = signatureIn;
        enableFlag = enableFlagIn;
        organizationDescription = organizationDescriptionIn;
    }

    bool EnforceTag(uint256 *hash = nullptr) const override;
    bool IsValidate() const override
    {
        if (tagValid == 0) {
            if (organizationName.length() < ORGANIZATION_NAME_MIN_SIZE || organizationName.length() > ORGANIZATION_NAME_MAX_SIZE) {
                tagValid = -1;
                return false;
            }
            if (organizationDescription.length() > ORGANIZATION_DESCRIPTION_MAX_SIZE) {
                tagValid = -1;
                return false;
            }
            if (ValidateSign()) {
                return true;
            }
            return false;
        }
        return (tagValid > 0);
    }
    bool IsReplaceable(const Coin& coin) const override;

    // la firma non comporta disuguaglianza verificare altri elementi
    bool operator==(const ScriptTag& o) const override
    {
        if (!o.IsTagType(NkbOrganization))
            return false;
        const OrganizationScript* op = static_cast<const OrganizationScript*>(&o);
        return (this->organizationName    == op->organizationName &&
                //std::string organizationDescription;       // serve ?
                this->organizationAddress == op->organizationAddress &&
                this->enableFlag == op->enableFlag);
    }

    bool operator!=(const ScriptTag& o) const override
    {
        return !(*this == o);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(LIMITED_STRING(organizationName, ORGANIZATION_NAME_MAX_SIZE));
        READWRITE(LIMITED_STRING(organizationDescription, ORGANIZATION_DESCRIPTION_MAX_SIZE));
        READWRITE(organizationAddress);
        READWRITE(enableFlag);
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(signature);
    }

    void SetNull()
    {
        ScriptTag::SetNull();
        organizationName = "";
        organizationDescription = "";
        organizationAddress.SetNull();
        enableFlag = 0;
        signature.clear();
    }

    bool MakeSign(CKey& key);
    bool ValidateSign() const;
    const std::vector<unsigned char> CreateTag() const override;
    const COutPoint GetTagOutpoint() const override;
    bool MakeTagTransaction(CMutableTransaction& txOut) const override;
    bool CheckTag(const CTransaction& tx, std::string &error) const override;
    AssetAmount GetAmount() const override { return AssetAmount(COIN_ASSET, 0); }
    CAmount GetFee() const override { return 0; }
    void ToUniv(UniValue& uv) const override;
    std::string ToString() const override;
};

class MintAssetScript : public ScriptTag
{
public:
    std::string assetName;              // Id, Nome univoco
    std::string assetDescription;       // serve ?
    CPubKey assetAddress;               // emittente
    WitnessV0KeyHash stateAddress;      // Stato (Opzionale e modificabile)
    WitnessV0KeyHash destinationAddress;// destinatario
    unsigned char issuanceFlag;         // Flag: open to re-issuance
    unsigned char extraWitness;         // Per future espansioni (coppie Nome:Valore)
    std::vector<unsigned char> signature;
    CAmount nAssetValue;                // numero di token
    // CAmount nValue;                     // Nokubit depositati
    // CAmount nFee;                       // Nokubit Fee emissione

private:
    CKeyID address;

public:
    MintAssetScript() : ScriptTag(NkbMintAsset)
    {
        SetNull();
    }

    ~MintAssetScript() { }

    MintAssetScript(
        const std::string& assetNameIn,
        const CPubKey& assetAddressIn,
        const std::vector<unsigned char>& signatureIn,
        const WitnessV0KeyHash& destinationAddressIn,
        const CAmount nAssetValueIn,
//        const CAmount nValueIn,
//        const CAmount nFeeIn = 0,
        const WitnessV0KeyHash& stateAddressIn = WitnessV0KeyHash(),
        const bool issuanceFlagIn = false,
        const std::string& assetDescriptionIn = ""
    )
    : ScriptTag(NkbMintAsset), extraWitness(0)
    {
        assetName = assetNameIn;
        assetAddress = assetAddressIn;
        signature = signatureIn;
        nAssetValue = nAssetValueIn;
        destinationAddress = destinationAddressIn;
//        nValue = nValueIn;
//        nFee = nFeeIn;
        stateAddress = stateAddressIn;
        issuanceFlag = issuanceFlagIn;
        assetDescription = assetDescriptionIn;
        address = assetAddress.GetID();
    }

    bool EnforceTag(uint256 *hash = nullptr) const override;
    bool IsValidate() const override
    {
        if (tagValid == 0) {
            if (assetName.length() < ASSET_NAME_MIN_SIZE || assetName.length() > ASSET_NAME_MAX_SIZE) {
                tagValid = -1;
                return false;
            }
            if (assetDescription.length() > ASSET_DESCRIPTION_MAX_SIZE) {
                tagValid = -1;
                return false;
            }
            // TODO NOKUBIT mettere destinationAddress obbligatorio
            if (ValidateSign()) {
                return true;
            }
            return false;
        }
        return (tagValid > 0);
    }
    bool IsReplaceable(const Coin& coin) const override;

    // la firma non comporta disuguaglianza verificare altri elementi
    bool operator==(const ScriptTag& o) const override
    {
        if (!o.IsTagType(NkbMintAsset))
            return false;
        const MintAssetScript* op = static_cast<const MintAssetScript*>(&o);
        return (this->assetName          == op->assetName &&
                //std::string assetDescription;       // serve ?
                this->assetAddress       == op->assetAddress &&
                //WitnessV0KeyHash stateAddress;   // Stato (Opzionale e modificabile)
                this->destinationAddress == op->destinationAddress &&
                this->issuanceFlag       == op->issuanceFlag &&
                //unsigned char extraWitness;         // Per future espansioni (coppie Nome:Valore)
                this->nAssetValue        == op->nAssetValue);
//                this->nValue             == op->nValue);
                //CAmount nFee;                       // Nokubit Fee emissione
    }

    bool operator!=(const ScriptTag& o) const override
    {
        return !(*this == o);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(LIMITED_STRING(assetName, ASSET_NAME_MAX_SIZE));
        READWRITE(LIMITED_STRING(assetDescription, ASSET_DESCRIPTION_MAX_SIZE));
        READWRITE(assetAddress);
        READWRITE(stateAddress);
        READWRITE(destinationAddress);
        READWRITE(nAssetValue);
//        READWRITE(nValue);
//        READWRITE(nFee);
        READWRITE(issuanceFlag);
        READWRITE(extraWitness);
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(signature);
        if (ser_action.ForRead()) {
            address = assetAddress.GetID();
        }
    }

    void SetNull()
    {
        ScriptTag::SetNull();
        assetName = "";
        assetDescription = "";
        const char *t = "";
        assetAddress.Set(t, t);
        stateAddress.SetNull();
        destinationAddress.SetNull();
        issuanceFlag = 0;
        extraWitness = 0;
        signature.clear();
        nAssetValue = -1;
//        nValue = -1;
//        nFee = 0;
    }

    bool MakeSign(const CKey& key);
    bool ValidateSign() const;
    const std::vector<unsigned char> CreateTag() const override;
    const COutPoint GetTagOutpoint() const override;
    bool MakeTagTransaction(CMutableTransaction& txOut) const override;
    bool CheckTag(const CTransaction& tx, std::string &error) const override;
    AssetAmount GetAmount() const override
    {
        return AssetAmount(assetName, nAssetValue);
//        return nValue;
    }
    // CAmount GetFee() const override
    // {
    //     return nFee;
    // }
    const WitnessV0KeyHash GetAddress() const { return WitnessV0KeyHash(address); }
    void ToUniv(UniValue& uv) const override;
    std::string ToString() const override;
};
/*
class AssetScript : public ScriptTag
{
public:
    std::string assetName;
    CAmount nValue;

public:
    AssetScript() : ScriptTag(NkbAsset)
    {
        SetNull();
    }

    ~AssetScript() { }

    AssetScript(const std::string& assetNameIn, const CAmount nValueIn)
    : ScriptTag(NkbAsset)
    {
        assetName = assetNameIn;
        nValue = nValueIn;
    }

    bool EnforceTag(uint256 *hash = nullptr) const override;
    bool IsValidate() const override
    {
        if (tagValid == 0) {
            if (assetName.length() < ASSET_NAME_MIN_SIZE || assetName.length() > ASSET_NAME_MAX_SIZE) {
                tagValid = -1;
                return false;
            }
            if (ValidateSign()) {
                return true;
            }
            return false;
        }
        return (tagValid > 0);
    }

    // la firma non comporta disuguaglianza verificare altri elementi
    bool operator==(const ScriptTag& o) const override
    {
        if (!o.IsTagType(NkbAsset))
            return false;
        const AssetScript* op = static_cast<const AssetScript*>(&o);
        return (this->assetName    == op->assetName &&
                this->nValue       == op->nValue);
    }

    bool operator!=(const ScriptTag& o) const override
    {
        return !(*this == o);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(assetName);
        READWRITE(nValue);
    }

    void SetNull()
    {
        assetName = "";
        nValue = -1;
    }

    bool ValidateSign() const;
    const std::vector<unsigned char> CreateTag() const override;
    const COutPoint GetTagOutpoint() const override;
    bool MakeTagTransaction(CMutableTransaction& txOut) const override;
    bool CheckTag(const CTransaction& tx, std::string &error) const override;
    AssetAmount GetAmount() const override
    {
        return nValue;
    }
    void ToUniv(UniValue& uv) const override;
    std::string ToString() const override;
};
*/

class PoSScript
{
private:
    CKeyID address;
    mutable int posValid;
    mutable uint256 blockHash;
    mutable uint256 posHash;

public:
    COutPoint stack;                    // tx stacked
    CAmount nStackValue;                // quota dello stack utilizzata
    CPubKey stackAddress;               // destinatario dello stack e firma PoS
    std::vector<unsigned char> signature;

    PoSScript() { SetNull(); }

    static PoSScript* ExtractTag(const CScriptWitness &script, const uint256& hash);

public:
    bool IsValidate() const
    {
        if (posValid == 0) {
            if (ValidateSign()) {
                return true;
            }
            return false;
        }
        return (posValid > 0);
    }

    // // la firma non comporta disuguaglianza verificare altri elementi
    // bool operator==(const ScriptTag& o) const override
    // {
    //     if (!o.IsTagType(NkbPoS))
    //         return false;
    //     const PoSScript* op = static_cast<const PoSScript*>(&o);
    //     return (this->stack         == op->stack &&
    //             this->nStackValue   == op->nStackValue &&
    //             this->stackAddress  == op->stackAddress);
    // }

    // bool operator!=(const ScriptTag& o) const override
    // {
    //     return !(*this == o);
    // }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(stack);
        READWRITE(nStackValue);
        READWRITE(stackAddress);
        if (s.GetType() & SER_GETHASH)
            READWRITE(blockHash);
        else
            READWRITE(signature);
        if (ser_action.ForRead()) {
            address = stackAddress.GetID();
        }
    }

    void SetNull(bool flag = true)
    {
        if (flag) posValid = 0;
        blockHash = uint256();
        posHash = uint256();
        stack.SetNull();
        nStackValue = -1;
        const char *t = "";
        stackAddress.Set(t, t);
        signature.clear();
    }

    bool ValidateSign() const;
    const uint256 GetPoSHash() const;
    AssetAmount GetAmount() const
    {
        return AssetAmount(COIN_ASSET, nStackValue);
    }
    void ToUniv(UniValue& uv) const;
    std::string ToString() const;
};

}   // asset_tag

#endif  // BITCOIN_NOKUBIT_TAGSCRIPT_H
