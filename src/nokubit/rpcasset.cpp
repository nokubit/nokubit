// TODO NOKUBIT: header


#include <base58.h>
#include <nokubit/asset.h>
#include <nokubit/tagscript.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <validation.h>
#include <validationinterface.h>
#include <net.h>
#include <policy/rbf.h>
#include <rpc/safemode.h>
#include <rpc/server.h>
#include <txmempool.h>
#include <uint256.h>
#include <utilstrencodings.h>
#include <utilmoneystr.h>

#include <future>

#include <util.h>

#include <stdint.h>

#include <univalue.h>


void TxToJSON(const CTransaction& tx, const uint256 hashBlock, UniValue& entry, bool include_hex = true);

UniValue createrawpegin(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 3)
        // TODO NOKUBIT:
        throw std::runtime_error(
            "createrawpegin pegin destaddress\n"
            "\nCreates a raw transaction to claim coins from the main chain by creating a withdraw transaction with the necessary metadata after the corresponding Bitcoin transaction.\n"
            "Note that this call will not sign the transaction.\n"
            "\nArguments:\n"
            "1. \"pegin proof\"           (object, required) A json object with pegin\n"
            "     {\n"
            "       \"chainGenesis\":\"hash\",  (string, required) The main chain genesis hash (in hex)\n"
            "       \"chainHeight\":height,     (numeric, required) The main chain transaction block height\n"
            "       \"chainTxId\":\"id\",       (string, required) The main chain transaction id pegging bitcoin to the mainchain address\n"
            "       \"chainVOut\":n,            (numeric, required) The main chain transaction output number\n"
            "       \"signature\":\"sig\",      (string, required) The hex-encoded pegin signature\n"
            "       \"nValue\": x.xxx           (numeric or string, required) numeric value (can be string) is the " + CURRENCY_UNIT + " amount.\n"
            "     } \n"
            "2. \"destaddress\"           (string, required) The destination address (Bech-32)\n"
            "3. \"fee\"                   (numeric or string, optional) Optional fee deducted from nValue. Numeric value (can be string) is the " + CURRENCY_UNIT + " amount.\n"
            "\nResult:\n"
            "\"transaction\"       (string) hex string of the transaction\n"
            "\nExamples:\n"
            // TODO NOKUBIT: riallineare esempi
            + HelpExampleCli("createrawpegin", "\"{\\\"chainGenesis\\\" : \\\"000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f\\\",\\\"chainTxId\\\" : \\\"2d1c0bb01d61fa420501edab64260664508f23fecfec35e014b628e7afa079e3\\\",\\\"chainVOut\\\" : 1,\\\"signature\\\" : \\\"3044022048d2928a673ec9c4aa8b8dd940189dc2e775b11e1dbd6c1a58dc0903b51a6a9e02202aba08bb385b68dfd150ca9402db73d5c451e7991ba0fefc86b334451c2a78f4\\\",\\\"nValue\\\":10}\" \"1PSSGeFHDnKNxiEyFrD1wcEaHr9hrQDDWc\"")
            + HelpExampleRpc("createrawpegin", "\"000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f\", 1, \"0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b00000001\", \"123.456\", \"0014058c769ffc7d12c35cddec87384506f536383f9c\"")
        );

    ObserveSafeMode();

LogPrint(BCLog::ASSET, "Received createrawpegin\n");
    RPCTypeCheck(request.params, {UniValue::VOBJ, UniValue::VSTR}, false);

    UniValue peginObj = request.params[0].get_obj();
    RPCTypeCheckObj(peginObj, {
            {"chainGenesis", UniValueType(UniValue::VSTR)},
            {"chainHeight", UniValueType(UniValue::VNUM)},
            {"chainTxId", UniValueType(UniValue::VSTR)},
            {"chainVOut", UniValueType(UniValue::VNUM)},
            {"signature", UniValueType(UniValue::VSTR)},
            {"nValue", UniValueType()}
        }, false, true);

    std::string sendTo = request.params[1].get_str();
    WitnessV0KeyHash destination;
    CTxDestination destination_u = DecodeDestination(sendTo);
    if (!IsValidDestination(destination_u)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Bitcoin address: ") + sendTo);
    }
    if (auto witness_id = boost::get<WitnessV0KeyHash>(&destination_u)) {
        destination = *witness_id;
    } else {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Bitcoin address: NOT P2WKH"));
    }

    uint256 genesis = ParseHashO(peginObj, "chainGenesis");

    int height = find_value(peginObj, "chainHeight").get_int();
    if (height <= 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, chainHeight must be strict positive");

    uint256 txid = ParseHashO(peginObj, "chainTxId");

    int nOutput = find_value(peginObj, "chainVOut").get_int();
    if (nOutput < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, chainVOut must be positive");

    CAmount nAmount = AmountFromValue(find_value(peginObj, "nValue"));

    std::vector<unsigned char> signature(ParseHexO(peginObj, "signature"));

    asset_tag::PeginScript* pegIn = new asset_tag::PeginScript(genesis, height, COutPoint(txid, nOutput), destination, signature, nAmount);
    if (!pegIn->IsValidate())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, signature failure");

    CMutableTransaction rawTx;
    if (!pegIn->MakeTagTransaction(rawTx))
        LogPrint(BCLog::ASSET, "\tPegin mainchain tx already inserted\n");

    CAmount nAmountFee = CONF_DEFAULT_PEGIN_FEE;
    if (!request.params[2].isNull()) {
        nAmountFee = AmountFromValue(request.params[2]);
    }
    // TODO NOKUBIT: vedi più avanti: calcolare dinamicamente la fee
    //  e/o aggiungere parametro in input con valore Fee
    if (nAmount < 2 * nAmountFee)
        nAmountFee = nAmount / 2;
    rawTx.vout[0].nValue -= nAmountFee;

//+-     // Estimate fee for transaction, decrement fee output(including estimated signature)
//+-     unsigned int nBytes = GetVirtualTransactionSize(rawTx)+(72/WITNESS_SCALE_FACTOR);
//+-     CAmount nFeeNeeded = CWallet::GetMinimumFee(nBytes, nTxConfirmTarget, mempool);
//+-
//+-     mtx.vout[0].nValue = mtx.vout[0].nValue.GetAmount() - nFeeNeeded;
//+-     mtx.vout[1].nValue = mtx.vout[1].nValue.GetAmount() + nFeeNeeded;

    UniValue ret(UniValue::VOBJ);

    // Return hex
    ret.push_back(Pair("tx", EncodeHexTx(rawTx)));
    // TODO NOKUBIT: hex e txj per debug
    std::vector<unsigned char> proof = pegIn->CreateTag();
    std::string strHex = HexStr(proof.begin(), proof.end());
    ret.push_back(Pair("hex", strHex));
    UniValue txj(UniValue::VOBJ);
    TxToJSON(rawTx, uint256(), txj);
    ret.push_back(Pair("txj", txj));

    std::promise<void> promise;
    CTransactionRef tx(MakeTransactionRef(std::move(rawTx)));
    const uint256& hashTx = tx->GetHash();
    CAmount nMaxRawTxFee = maxTxFee;

    { // cs_main scope
    LOCK(cs_main);
    CCoinsViewCache &view = *pcoinsTip;
    bool fHaveChain = false;
    for (size_t o = 0; !fHaveChain && o < tx->vout.size(); o++) {
        const Coin& existingCoin = view.AccessCoin(COutPoint(hashTx, o));
        fHaveChain = !existingCoin.IsSpent();
    }
    bool fHaveMempool = mempool.exists(hashTx);
    if (!fHaveMempool && !fHaveChain) {
        // push to local node and sync with wallets
        CValidationState state;
        bool fMissingInputs;
        if (!AcceptToMemoryPool(mempool, state, std::move(tx), &fMissingInputs,
                                nullptr /* plTxnReplaced */, false /* bypass_limits */, nMaxRawTxFee)) {
            if (state.IsInvalid()) {
                throw JSONRPCError(RPC_TRANSACTION_REJECTED, strprintf("%i: %s", state.GetRejectCode(), state.GetRejectReason()));
            } else {
                if (fMissingInputs) {
                    throw JSONRPCError(RPC_TRANSACTION_ERROR, "Missing inputs");
                }
                throw JSONRPCError(RPC_TRANSACTION_ERROR, state.GetRejectReason());
            }
        } else {
            // If wallet is enabled, ensure that the wallet has been made aware
            // of the new transaction prior to returning. This prevents a race
            // where a user might call sendrawtransaction with a transaction
            // to/from their wallet, immediately call some wallet RPC, and get
            // a stale result because callbacks have not yet been processed.
            CallFunctionInValidationInterfaceQueue([&promise] {
                promise.set_value();
            });
        }
    } else if (fHaveChain) {
        throw JSONRPCError(RPC_TRANSACTION_ALREADY_IN_CHAIN, "transaction already in block chain");
    } else {
        // Make sure we don't block forever if re-sending
        // a transaction already in mempool.
        promise.set_value();
    }

    } // cs_main

    promise.get_future().wait();

    if(!g_connman)
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");

    CInv inv(MSG_TX, hashTx);
    g_connman->ForEachNode([&inv](CNode* pnode)
    {
        pnode->PushInventory(inv);
    });

    return ret;
}

UniValue createrawstate(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 6)
        // TODO NOKUBIT:
        throw std::runtime_error(
            "createrawstate state [{\"txid\":\"id\",\"vout\":n},...] onoff\n"
            "\nCreates a raw transaction to add, enable or disable an organization public key by creating a transaction with the necessary metadata.\n"
            "Note that this call will not sign the transaction.\n"
            "\nArguments:\n"
            "1. \"state proof\"         (object, required) A json object with state\n"
            "     {\n"
            "       \"name\":\"label\",         (string, required) The name of organization\n"
            "       \"description\":\"desc\",   (string, optional) Optional description\n"
            "       \"stateAddress\":\"owner\", (string, required) The organization's owner Bech32 Address\n"
            "       \"signature\":\"sig\",      (string, required) The hex-encoded organization signature\n"
            "     } \n"
            "2. \"inputs\"              (array, required) A json array of json objects\n"
            "     [\n"
            "       {\n"
            "         \"txid\":\"id\",          (string, required) The transaction id\n"
            "         \"vout\":n,               (numeric, required) The output number\n"
            "         \"sequence\":n            (numeric, optional) The sequence number\n"
            "       } \n"
            "       ,...\n"
            "     ]\n"
            "3. \"onoff\"               (string, optional, default=enable) 'enable' or 'disable' organization public key\n"
            "4. \"output\"              (object, optional) a json object with fee change output\n"
            "    {\n"
            "      \"address\": x.xxx           (numeric or string, required) The key is the bitcoin address, the numeric value (can be string) is the " + CURRENCY_UNIT + " fee change\n"
            "    }\n"
            "5. locktime                (numeric, optional, default=0) Raw locktime. Non-0 value also locktime-activates inputs\n"
            "6. replaceable             (boolean, optional, default=false) Marks this transaction as BIP125 replaceable.\n"
            "\nResult:\n"
            "\"transaction\"       (string) hex string of the transaction\n"
            "\nExamples:\n"
            // TODO NOKUBIT: riallineare esempi
            + HelpExampleCli("createrawstate", "\"{\\\"name\\\" : \\\"Owner\\\",\\\"description\\\" : \\\"Owner creation\\\",\\\"stateAddress\\\" : \\\"xxxxxxx\\\",\\\"signature\\\" : \\\"3044022048d2928a673ec9c4aa8b8dd940189dc2e775b11e1dbd6c1a58dc0903b51a6a9e02202aba08bb385b68dfd150ca9402db73d5c451e7991ba0fefc86b334451c2a78f4\\\"}\" \"enable\"")
            + HelpExampleRpc("createrawstate", "\"000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f\", 1, \"0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b00000001\", \"123.456\", \"enable\"")
        );

LogPrint(BCLog::ASSET, "Received createrawstate\n");
    RPCTypeCheck(request.params, {UniValue::VOBJ, UniValue::VARR, UniValue::VSTR, UniValue::VOBJ, UniValue::VNUM, UniValue::VBOOL}, true);
    if (request.params[0].isNull() || request.params[1].isNull())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, arguments 1 and 2 must be non-null");

    UniValue stateObj = request.params[0].get_obj();
    RPCTypeCheckObj(stateObj, {
            {"name", UniValueType(UniValue::VSTR)},
            {"description", UniValueType(UniValue::VSTR)},
            {"stateAddress", UniValueType(UniValue::VSTR)},
            {"signature", UniValueType(UniValue::VSTR)}
        }, true, true);

    std::string stateName(find_value(stateObj, "name").getValStr());
    if (stateName.empty()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, missing 'name' key");
    }
    if (stateName.length() < asset_tag::ORGANIZATION_NAME_MIN_SIZE || stateName.length() > asset_tag::ORGANIZATION_NAME_MAX_SIZE) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid parameter, name length %d, %s", stateName.length(), stateName));
    }

    std::string description(find_value(stateObj, "description").getValStr());
    if (description.length() > asset_tag::ORGANIZATION_DESCRIPTION_MAX_SIZE) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid parameter, description length %d, %s", description.length(), description));
    }

    WitnessV0KeyHash stateAddress;
    CTxDestination stateAddress_u = DecodeDestination(find_value(stateObj, "stateAddress").getValStr());
    if (!IsValidDestination(stateAddress_u)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Bitcoin address: ") + find_value(stateObj, "stateAddress").getValStr());
    }
    if (auto witness_id = boost::get<WitnessV0KeyHash>(&stateAddress_u)) {
        stateAddress = *witness_id;
    } else {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Bitcoin address: NOT P2WKH"));
    }

    std::vector<unsigned char> signature(ParseHexO(stateObj, "signature"));

    UniValue inputs = request.params[1].get_array();

    bool onoff = true;
    std::string enable = request.params[2].get_str();
    if (enable.length() == 0 || enable == "enable" || enable == "on") {
        onoff = true;
    } else if (enable == "disable" || enable == "off") {
        onoff = false;
    } else {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid onoff flag: ") + enable);
    }

    CMutableTransaction rawTx;
    asset_tag::OrganizationScript* state = new asset_tag::OrganizationScript(stateName, stateAddress, signature, onoff, description);
    if (!state->IsValidate())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, signature failure");

    if (!state->MakeTagTransaction(rawTx))
        LogPrint(BCLog::ASSET, "\tOrganization name tx already inserted\n");

    UniValue output(UniValue::VOBJ);
    if (!request.params[3].isNull()) {
        output = request.params[3].get_obj();
        if (output.getKeys().size() > 1) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Only one change address is accepted");
        }
    }

    if (!request.params[4].isNull()) {
        int64_t nLockTime = request.params[4].get_int64();
        if (nLockTime < 0 || nLockTime > std::numeric_limits<uint32_t>::max())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, locktime out of range");
        rawTx.nLockTime = nLockTime;
    }

    bool rbfOptIn = request.params[5].isTrue();

    for (unsigned int idx = 0; idx < inputs.size(); idx++) {
        const UniValue& input = inputs[idx];
        const UniValue& o = input.get_obj();

        uint256 txid = ParseHashO(o, "txid");

        const UniValue& vout_v = find_value(o, "vout");
        if (!vout_v.isNum())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, missing vout key");
        int nOutput = vout_v.get_int();
        if (nOutput < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout must be positive");

        uint32_t nSequence;
        if (rbfOptIn) {
            nSequence = MAX_BIP125_RBF_SEQUENCE;
        } else if (rawTx.nLockTime) {
            nSequence = std::numeric_limits<uint32_t>::max() - 1;
        } else {
            nSequence = std::numeric_limits<uint32_t>::max();
        }

        // set the sequence number if passed in the parameters object
        const UniValue& sequenceObj = find_value(o, "sequence");
        if (sequenceObj.isNum()) {
            int64_t seqNr64 = sequenceObj.get_int64();
            if (seqNr64 < 0 || seqNr64 > std::numeric_limits<uint32_t>::max()) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, sequence number is out of range");
            } else {
                nSequence = (uint32_t)seqNr64;
            }
        }

        CTxIn in(COutPoint(txid, nOutput), CScript(), nSequence);
        in.scriptWitness.stack.resize(1);

        rawTx.vin.push_back(in);
    }

    const std::vector<std::string>& addrList = output.getKeys();
    if (addrList.size() > 0) {
        const std::string& name_ = addrList[0];

        CTxDestination destination = DecodeDestination(name_);
        if (!IsValidDestination(destination)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Bitcoin address: ") + name_);
        }
        // NOKUBIT: verifica BECH-32
        if (!boost::get<WitnessV0KeyHash>(&destination)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Bitcoin address: NOT P2WKH") + name_);
        }

        CScript scriptPubKey = GetScriptForDestination(destination);
        CAmount nAmount = AmountFromValue(output[name_]);

        CTxOut out(nAmount, scriptPubKey);
        rawTx.vout.push_back(out);
    }

    if (!request.params[3].isNull() && rbfOptIn != SignalsOptInRBF(rawTx)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter combination: Sequence number(s) contradict replaceable option");
    }

    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("tx", EncodeHexTx(rawTx)));

    // TODO NOKUBIT: hex e txj per debug
    std::vector<unsigned char> proof = state->CreateTag();
    std::string strHex = HexStr(proof.begin(), proof.end());
    ret.push_back(Pair("hex", strHex));
    UniValue txj(UniValue::VOBJ);
    TxToJSON(rawTx, uint256(), txj);
    ret.push_back(Pair("txj", txj));

    return ret;
}

UniValue createrawasset(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 4 || request.params.size() > 7)
        // TODO NOKUBIT:
        throw std::runtime_error(
            "createrawasset {asset} [{\"txid\":\"id\",\"vout\":n},...] destaddress assetamount ( fee ) ( locktime ) ( replaceable )\n"
            "\nCreate a transaction spending the given inputs and creating new asset.\n"

            "\nArguments:\n"
            "1. \"asset proof\"             (object, required) A json object with asset definition\n"
            "     {\n"
            "       \"assetName\":\"name\",         (string, required) The asset denomination\n"
            "       \"assetDescription\":\"desc\",  (string, optional) The asset description\n"
            "       \"assetAddress\":\"owner\",     (string, required) The asset's owner Public Key\n"
            "       \"stateAddress\":\"state\",     (string, optional) The asset's state address (Bech-32)\n"
            "       \"signature\":\"sig\",          (string, required) The hex-encoded asset signature signed by owner\n"
            "       \"peggedValue\":value,          (numeric or string, required) numeric value (can be string) is the " + CURRENCY_UNIT + " amount blocket to asset.\n"
            "       \"assetFee\":fee,               (numeric or string, required) numeric value (can be string) is the " + CURRENCY_UNIT + " amount asset fee.\n"
            "       \"issuanceFlag\":flag           (bool, optional, default=false) Whether to allow you to reissue the asset if in wallet using `reissueasset`.\n"
            "     } \n"
            "2. \"inputs\"                  (array, required) A json array of json objects\n"
            "     [\n"
            "       {\n"
            "         \"txid\":\"id\",      (string, required) The transaction id\n"
            "         \"vout\":n,           (numeric, required) The output number\n"
            "         \"sequence\":n        (numeric, optional) The sequence number\n"
            "       } \n"
            "       ,...\n"
            "     ]\n"
            "3. \"destaddress\"             (string, required) The destination address (Bech-32)\n"
            "4. assetamount                 (numeric or string, required) Amount of asset to generate.\n"
            "5. \"output\"                  (object, optional) a json object with fee change output\n"
            "    {\n"
            "      \"address\": x.xxx           (numeric or string, required) The key is the bitcoin address, the numeric value (can be string) is the " + CURRENCY_UNIT + " fee change\n"
            "    }\n"
            "6. locktime                    (numeric, optional, default=0) Raw locktime. Non-0 value also locktime-activates inputs\n"
            "7. replaceable                 (boolean, optional, default=false) Marks this transaction as BIP125 replaceable.\n"
            "                                   Allows this transaction to be replaced by a transaction with higher fees. If provided, it is an error if explicit sequence numbers are incompatible.\n"
            "\nResult:\n"
            // TODO NOKUBIT: riallineare esempi
            "{                          (json object)\n"
            "  \"tx\":\"hex\",          (string) hex string of the transaction\n"
            "  \"txid\":\"<txid>\",     (string) Transaction id for issuance.\n"
            "  \"vin\":n,               (numeric) The input position of the issuance in the transaction.\n"
            "}\n"
            "\nExamples:\n"
            // TODO NOKUBIT: riallineare esempi
            + HelpExampleCli("createrawasset", "10 0")
            + HelpExampleRpc("createrawasset", "10, 0")
        );

LogPrint(BCLog::ASSET, "Received createrawasset\n");
    RPCTypeCheck(request.params, {UniValue::VOBJ, UniValue::VARR, UniValue::VSTR, UniValue::VNUM, UniValue::VOBJ, UniValue::VNUM, UniValue::VBOOL}, true);
    if (request.params[0].isNull() || request.params[1].isNull() || request.params[2].isNull() || request.params[3].isNull())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, arguments 1, 2, 3 and 4 must be non-null");

    UniValue assetObj = request.params[0].get_obj();
    RPCTypeCheckObj(assetObj, {
            {"assetName", UniValueType(UniValue::VSTR)},
            {"assetDescription", UniValueType(UniValue::VSTR)},
            {"assetAddress", UniValueType(UniValue::VSTR)},
            {"stateAddress", UniValueType(UniValue::VSTR)},
            {"signature", UniValueType(UniValue::VSTR)},
            {"peggedValue", UniValueType()},
            {"assetFee", UniValueType()},
            {"issuanceFlag", UniValueType(UniValue::VBOOL)}
        }, true, true);

    std::string assetName(find_value(assetObj, "assetName").getValStr());
    if (assetName.empty()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, missing 'assetName' key");
    }
    if (assetName.length() < asset_tag::ASSET_NAME_MIN_SIZE || assetName.length() > asset_tag::ASSET_NAME_MAX_SIZE) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid parameter, assetName length %d, %s", assetName.length(), assetName));
    }

    std::string description(find_value(assetObj, "assetDescription").getValStr());
    if (description.length() > asset_tag::ASSET_DESCRIPTION_MAX_SIZE) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid parameter, asset description length %d, %s", description.length(), description));
    }

    CPubKey assetAddress(ParseHexO(assetObj, "assetAddress"));
    if (!assetAddress.IsFullyValid()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Bitcoin PubKey");
    }

    WitnessV0KeyHash stateAddress;
    std::string stateAddress_s = find_value(assetObj, "stateAddress").getValStr();
    if (!stateAddress_s.empty()) {
        CTxDestination stateAddress_u = DecodeDestination(stateAddress_s);
        if (!IsValidDestination(stateAddress_u)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Bitcoin address: ") + stateAddress_s);
        }
        if (auto witness_id = boost::get<WitnessV0KeyHash>(&stateAddress_u)) {
            stateAddress = *witness_id;
        } else {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Bitcoin address: NOT P2WKH"));
        }
    }

    std::vector<unsigned char> signature(ParseHexO(assetObj, "signature"));

    CAmount nPeggedAmount = AmountFromValue(find_value(assetObj, "peggedValue"));
    if (nPeggedAmount == 0) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Issuance must have non-zero pegged amount values");
    }

    CAmount nAssetFee = AmountFromValue(find_value(assetObj, "assetFee"));
    // NOKUBIT: vedi più avanti: calcolare dinamicamente la fee
    if (nAssetFee < 1000) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Issuance must have minimum fee values");
    }

    bool issuanceFlag = find_value(assetObj, "issuanceFlag").isTrue();

    UniValue inputs = request.params[1].get_array();

    std::string sendTo = request.params[2].get_str();
    WitnessV0KeyHash assetDestination;
    CTxDestination assetDestination_u = DecodeDestination(sendTo);
    if (!IsValidDestination(assetDestination_u)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Bitcoin address: ") + sendTo);
    }
    if (auto witness_id = boost::get<WitnessV0KeyHash>(&assetDestination_u)) {
        assetDestination = *witness_id;
    } else {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Bitcoin address: NOT P2WKH"));
    }
    CScript scriptTokenPubKey = GetScriptForDestination(assetDestination);

    CAmount nTokenAmount = request.params[3].get_int64();
    if (nTokenAmount <= 0) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Issuance must have non-zero positive token amount values");
    }

    CMutableTransaction rawTx;
    //asset_tag::MintAssetScript* asset = new asset_tag::MintAssetScript(assetName, assetAddress, signature, nPeggedAmount, nAssetFee, stateAddress, issuanceFlag, description);
    asset_tag::MintAssetScript* asset = new asset_tag::MintAssetScript(assetName, assetAddress, signature, assetDestination, nTokenAmount, stateAddress, issuanceFlag, description);
    if (!asset->IsValidate())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, signature failure");

    if (!asset->MakeTagTransaction(rawTx))
        LogPrint(BCLog::ASSET, "\tAsset name tx already inserted\n");

    UniValue output(UniValue::VOBJ);
    if (!request.params[4].isNull()) {
        output = request.params[4].get_obj();
        if (output.getKeys().size() > 1) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Only one change address is accepted");
        }
    }

    if (!request.params[5].isNull()) {
        int64_t nLockTime = request.params[5].get_int64();
        if (nLockTime < 0 || nLockTime > std::numeric_limits<uint32_t>::max())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, locktime out of range");
        rawTx.nLockTime = nLockTime;
    }

    bool rbfOptIn = request.params[6].isTrue();

    int version;
    std::vector<unsigned char> witnessProgram;
    // NOKUBIT: verifica BECH-32
    // Only process witness v0 programs
    if (!scriptTokenPubKey.IsWitnessProgram(version, witnessProgram) || version != 0) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Invalid parameter, witnessProgram failure");
    }
    rawTx.vout[0].scriptPubKey = scriptTokenPubKey;

    for (unsigned int idx = 0; idx < inputs.size(); idx++) {
        const UniValue& input = inputs[idx];
        const UniValue& o = input.get_obj();

        uint256 txid = ParseHashO(o, "txid");

        const UniValue& vout_v = find_value(o, "vout");
        if (!vout_v.isNum())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, missing vout key");
        int nOutput = vout_v.get_int();
        if (nOutput < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout must be positive");

        uint32_t nSequence;
        if (rbfOptIn) {
            nSequence = MAX_BIP125_RBF_SEQUENCE;
        } else if (rawTx.nLockTime) {
            nSequence = std::numeric_limits<uint32_t>::max() - 1;
        } else {
            nSequence = std::numeric_limits<uint32_t>::max();
        }

        // set the sequence number if passed in the parameters object
        const UniValue& sequenceObj = find_value(o, "sequence");
        if (sequenceObj.isNum()) {
            int64_t seqNr64 = sequenceObj.get_int64();
            if (seqNr64 < 0 || seqNr64 > std::numeric_limits<uint32_t>::max()) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, sequence number is out of range");
            } else {
                nSequence = (uint32_t)seqNr64;
            }
        }

        CTxIn in(COutPoint(txid, nOutput), CScript(), nSequence);
        in.scriptWitness.stack.resize(1);

        rawTx.vin.push_back(in);
    }

    std::vector<std::string> addrList = output.getKeys();
    if (addrList.size() > 0) {
        const std::string& name_ = addrList[0];

        CTxDestination destination = DecodeDestination(name_);
        if (!IsValidDestination(destination)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Bitcoin address: ") + name_);
        }
        // NOKUBIT: verifica BECH-32
        if (!boost::get<WitnessV0KeyHash>(&destination)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Bitcoin address: NOT P2WKH") + name_);
        }

        CScript scriptPubKey = GetScriptForDestination(destination);
        CAmount nAmount = AmountFromValue(output[name_]);

        CTxOut out(nAmount, scriptPubKey);
        rawTx.vout.push_back(out);
    }

    if (!request.params[6].isNull() && rbfOptIn != SignalsOptInRBF(rawTx)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter combination: Sequence number(s) contradict replaceable option");
    }

    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("tx", EncodeHexTx(rawTx)));

    ret.push_back(Pair("txid", rawTx.GetHash().GetHex()));
    ret.push_back(Pair("vin", 0));

    // TODO NOKUBIT: hex e txj per debug
    std::vector<unsigned char> proof = asset->CreateTag();
    std::string strHex = HexStr(proof.begin(), proof.end());
    ret.push_back(Pair("hex", strHex));
    UniValue txj(UniValue::VOBJ);
    TxToJSON(rawTx, uint256(), txj);
    ret.push_back(Pair("txj", txj));

    return ret;
}


//#include <wallet/rpcwallet.h>
#include <wallet/wallet.h>
#include <wallet/coincontrol.h>

static void SendAsset(CWallet * const pwallet, const CTxDestination &address, CAmount nValue, bool fSubtractFeeFromAmount, CWalletTx& wtxNew, const CCoinControl& coin_control)
{
    // NOKUBIT: Nokubit AssetCoin
    asset_tag::CAmountMap curBalanceMap = pwallet->GetBalance();
    CAmount curBalance = curBalanceMap[asset_tag::COIN_ASSET];

    // Check amount
    if (nValue <= 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid amount");

    if (nValue > curBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds");

    if (pwallet->GetBroadcastTransactions() && !g_connman) {
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");
    }

    // Parse Bitcoin address
    CScript scriptPubKey = GetScriptForDestination(address);

    // Create and send the transaction
    CReserveKey reservekey(pwallet);
    CAmount nFeeRequired;
    std::string strError;
    std::vector<CRecipient> vecSend;
    int nChangePosRet = -1;
    CRecipient recipient = {scriptPubKey, nValue, fSubtractFeeFromAmount};
    vecSend.push_back(recipient);
    if (!pwallet->CreateTransaction(vecSend, wtxNew, reservekey, nFeeRequired, nChangePosRet, strError, coin_control)) {
        if (!fSubtractFeeFromAmount && nValue + nFeeRequired > curBalance)
            strError = strprintf("Error: This transaction requires a transaction fee of at least %s", FormatMoney(nFeeRequired));
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    // CValidationState state;
    // if (!pwallet->CommitTransaction(wtxNew, reservekey, g_connman.get(), state)) {
    //     strError = strprintf("Error: The transaction was rejected! Reason given: %s", state.GetRejectReason());
    //     throw JSONRPCError(RPC_WALLET_ERROR, strError);
    // }
}

UniValue issueasset(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 3 || request.params.size() > 6)
        // TODO NOKUBIT:
        throw std::runtime_error(
            "issueasset {asset} destaddress peggedamount assetamount ( fee ) ( locktime ) ( replaceable )\n"
            "\nCreate an asset. Must have funds in wallet to do so. Returns asset hex id.\n"
            + HelpRequiringPassphrase(pwallet) +
            "\nArguments:\n"
            "1. \"asset proof\"             (object, required) A json object with asset definition\n"
            "     {\n"
            "       \"assetName\":\"name\",         (string, required) The asset denomination\n"
            "       \"assetDescription\":\"desc\",  (string, optional) The asset description\n"
            "       \"assetAddress\":\"owner\",     (string, optional) The asset's owner Public Key\n"
            "       \"stateAddress\":\"state\",     (string, optional) The asset's state address (Bech-32)\n"
            "       \"peggedValue\":value,          (numeric or string, required) numeric value (can be string) is the " + CURRENCY_UNIT + " amount blocket to asset.\n"
            "       \"assetFee\":fee,               (numeric or string, required) numeric value (can be string) is the " + CURRENCY_UNIT + " amount asset fee.\n"
            "       \"issuanceFlag\":flag           (bool, optional, default=false) Whether to allow you to reissue the asset if in wallet using `reissueasset`.\n"
            "     } \n"
            "2. \"destaddress\"             (string, required) The destination address (Bech-32)\n"
            "3. assetamount                 (numeric or string, required) Amount of asset to generate.\n"
            "4. fee                         (numeric or string, optional) numeric value (can be string) is the " + CURRENCY_UNIT + " amount.\n"
            "5. locktime                    (numeric, optional, default=0) Raw locktime. Non-0 value also locktime-activates inputs\n"
            "6. replaceable                 (boolean, optional, default=false) Marks this transaction as BIP125 replaceable.\n"
            "                               Allows this transaction to be replaced by a transaction with higher fees. If provided, it is an error if explicit sequence numbers are incompatible.\n"
            "\nResult:\n"
            // TODO
            "{                          (json object)\n"
            "  \"owner\":\"pubkey\",    (string) The asset's owner Public Key\n"
            "  \"txid\":\"<txid>\",     (string) Transaction id of issuance.\n"
            "  \"vin\":n,               (numeric) The input position of the issuance in the transaction.\n"
            "}\n"
            "\nExamples:\n"
            // TODO
            + HelpExampleCli("issueasset", "10 0")
            + HelpExampleRpc("issueasset", "10, 0")
        );

    ObserveSafeMode();

LogPrint(BCLog::ASSET, "Received issueasset\n");
    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    RPCTypeCheck(request.params, {UniValue::VOBJ, UniValue::VSTR, UniValue::VNUM, UniValue::VNUM, UniValue::VNUM, UniValue::VBOOL}, true);
    if (request.params[0].isNull() || request.params[1].isNull() || request.params[2].isNull() || request.params[3].isNull())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, arguments 1, 2, 3 and 4 must be non-null");

    UniValue assetObj = request.params[0].get_obj();
    RPCTypeCheckObj(assetObj, {
            {"assetName", UniValueType(UniValue::VSTR)},
            {"assetDescription", UniValueType(UniValue::VSTR)},
            {"assetAddress", UniValueType(UniValue::VSTR)},
            {"stateAddress", UniValueType(UniValue::VSTR)},
            {"peggedValue", UniValueType()},
            {"assetFee", UniValueType()},
            {"issuanceFlag", UniValueType(UniValue::VBOOL)}
        }, true, true);

    std::string assetName(find_value(assetObj, "assetName").getValStr());
    if (assetName.empty()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, missing 'assetName' key");
    }
    if (assetName.length() < asset_tag::ASSET_NAME_MIN_SIZE || assetName.length() > asset_tag::ASSET_NAME_MAX_SIZE) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid parameter, assetName length %d, %s", assetName.length(), assetName));
    }

    std::string description(find_value(assetObj, "assetDescription").getValStr());
    if (description.length() > asset_tag::ASSET_DESCRIPTION_MAX_SIZE) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid parameter, asset description length %d, %s", description.length(), description));
    }

    CPubKey assetAddress;
    if (!find_value(assetObj, "assetAddress").isNull()) {
        const std::vector<unsigned char>& vch = ParseHexO(assetObj, "assetAddress");
        assetAddress.Set(vch.begin(), vch.end());
        if (!assetAddress.IsFullyValid()) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Bitcoin PubKey");
        }
    }

    WitnessV0KeyHash stateAddress;
    std::string stateAddress_s = find_value(assetObj, "stateAddress").getValStr();
    if (!stateAddress_s.empty()) {
        CTxDestination stateAddress_u = DecodeDestination(stateAddress_s);
        if (!IsValidDestination(stateAddress_u)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Bitcoin address: ") + stateAddress_s);
        }
        if (auto witness_id = boost::get<WitnessV0KeyHash>(&stateAddress_u)) {
            stateAddress = *witness_id;
        } else {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Bitcoin address: NOT P2WKH"));
        }
    }

    CAmount nPeggedAmount = AmountFromValue(find_value(assetObj, "peggedValue"));
    if (nPeggedAmount == 0) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Issuance must have non-zero pegged amount values");
    }

    CAmount nAssetFee = AmountFromValue(find_value(assetObj, "assetFee"));
    // NOKUBIT: vedi più avanti: calcolare dinamicamente la fee
    if (nAssetFee < 1000) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Issuance must have minimum fee values");
    }

    bool issuanceFlag = find_value(assetObj, "issuanceFlag").isTrue();

    std::string sendTo = request.params[1].get_str();
    WitnessV0KeyHash assetDestination;
    CTxDestination assetDestination_u = DecodeDestination(sendTo);
    if (!IsValidDestination(assetDestination_u)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Bitcoin address: ") + sendTo);
    }
    if (auto witness_id = boost::get<WitnessV0KeyHash>(&assetDestination_u)) {
        assetDestination = *witness_id;
    } else {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Bitcoin address: NOT P2WKH"));
    }
    CScript scriptTokenPubKey = GetScriptForDestination(assetDestination);

    CAmount nTokenAmount = AmountFromValue(request.params[2]);
    if (nTokenAmount == 0) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Issuance must have non-zero positive token amount values");
    }

    CAmount nFee = 0;
    if (!request.params[3].isNull()) {
        nFee = AmountFromValue(request.params[3]);
        if (nFee < 4000) {
            throw JSONRPCError(RPC_TYPE_ERROR, "Issuance must have minimum fee values");
        }
    }
    if (nFee == 0) {
        // NOKUBIT: vedi più avanti: calcolare dinamicamente la fee
        nFee = 4000;
    }
    CAmount nAmount = nPeggedAmount + nFee;

    LOCK2(cs_main, pwallet->cs_wallet);

    if (!pwallet->IsLocked()) {
         pwallet->TopUpKeyPool();
    }

    OutputType output_type = OUTPUT_TYPE_BECH32;
    if (assetAddress.size() == 0) {
        // Generate a new key that is added to wallet
        if (!pwallet->GetKeyFromPool(assetAddress)) {
            throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
        }
        pwallet->LearnRelatedScripts(assetAddress, output_type);
    }
    CTxDestination dest = GetDestinationForKey(assetAddress, output_type);
    pwallet->SetAddressBook(dest, "", "receive");

    CKey key;
    if (!pwallet->GetKey(assetAddress.GetID(), key)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Private key not available");
    }

    std::vector<unsigned char> signature;
    //asset_tag::MintAssetScript* asset = new asset_tag::MintAssetScript(assetName, assetAddress, signature, nPeggedAmount, nAssetFee, stateAddress, issuanceFlag, description);
    asset_tag::MintAssetScript* asset = new asset_tag::MintAssetScript(assetName, assetAddress, signature, assetDestination, nPeggedAmount, stateAddress, issuanceFlag, description);

    if (!asset->MakeSign(key))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Sign failed");
    if (!asset->IsValidate())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, signature failure");


    // Wallet comments
    CWalletTx wtx;

    bool fSubtractFeeFromAmount = true;

    CCoinControl coin_control;
    if (!request.params[5].isNull()) {
        coin_control.signalRbf = request.params[5].get_bool();
    }

    // if (!request.params[6].isNull()) {
    //     coin_control.m_confirm_target = ParseConfirmTarget(request.params[6]);
    // }

    // if (!request.params[7].isNull()) {
    //     if (!FeeModeFromString(request.params[7].get_str(), coin_control.m_fee_mode)) {
    //         throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid estimate_mode parameter");
    //     }
    // }

    EnsureWalletIsUnlocked(pwallet);

    SendAsset(pwallet, dest, nAmount, fSubtractFeeFromAmount, wtx, coin_control);

    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("tx", EncodeHexTx(*wtx.tx)));
    ret.push_back(Pair("txid", wtx.tx->GetHash().GetHex()));
    ret.push_back(Pair("vin", 0));
    ret.push_back(Pair("assetAddress", HexStr(assetAddress.begin(), assetAddress.end())));
    ret.push_back(Pair("assetSK", HexStr(key.GetPrivKey())));
    return ret;
}

UniValue reissueasset(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 0)
        // TODO NOKUBIT:
        throw std::runtime_error(
            "reissueasset asset assetamount\n"
            "\nCreate more of an already issued asset. Must have reissuance token in wallet to do so. Reissuing does not affect your reissuance token balance, only asset.\n"
            "\nArguments:\n"
            "1. \"asset\"                 (string, required) The asset you want to re-issue. The corresponding token must be in your wallet.\n"
            "2. \"assetamount\"           (numeric or string, required) Amount of additional asset to generate.\n"
            "\nResult:\n"
            "{                        (json object)\n"
            "  \"txid\":\"<txid>\",   (string) Transaction id for issuance.\n"
            "  \"vin\":\"n\",         (numeric) The input position of the issuance in the transaction.\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("reissueasset", "<asset> 0")
            + HelpExampleRpc("reissueasset", "<asset>, 0")
        );

    // LOCK2(cs_main, pwalletMain->cs_wallet);

    // std::string assetstr = request.params[0].get_str();

    // CAsset asset = GetAssetFromString(assetstr);

    // CAmount nAmount = AmountFromValue(request.params[1]);
    // if (nAmount <= 0)
    //     throw JSONRPCError(RPC_TYPE_ERROR, "Reissuance must create a non-zero amount.");

    // if (!pwalletMain->IsLocked())
    //     pwalletMain->TopUpKeyPool();

    // // Find the entropy and reissuance token in wallet
    // std::map<uint256, std::pair<CAsset, CAsset> > tokenMap = pwalletMain->GetReissuanceTokenTypes();
    // CAsset reissuanceToken;
    // uint256 entropy;
    // for (const auto& it : tokenMap) {
    //     if (it.second.second == asset) {
    //         reissuanceToken = it.second.first;
    //         entropy = it.first;
    //     }
    //     if (it.second.first == asset) {
    //         throw JSONRPCError(RPC_WALLET_ERROR, "Asset given is a reissuance token type and can not be reissued.");
    //     }
    // }
    // if (reissuanceToken.IsNull()) {
    //     throw JSONRPCError(RPC_WALLET_ERROR, "Asset reissuance token definition could not be found in wallet.");
    // }

    // CPubKey newKey;
    // CKeyID keyID;
    // CBitcoinAddress assetAddr;
    // CPubKey assetKey;
    // CBitcoinAddress tokenAddr;
    // CPubKey tokenKey;

    // // Add destination for the to-be-created asset
    // if (!pwalletMain->GetKeyFromPool(newKey))
    //     throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
    // keyID = newKey.GetID();
    // pwalletMain->SetAddressBook(keyID, "", "receive");
    // assetAddr = CBitcoinAddress(keyID);
    // assetKey = pwalletMain->GetBlindingPubKey(GetScriptForDestination(assetAddr.Get()));

    // // Add destination for tokens we are moving
    // if (!pwalletMain->GetKeyFromPool(newKey))
    //     throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
    // keyID = newKey.GetID();
    // pwalletMain->SetAddressBook(keyID, "", "receive");
    // tokenAddr = CBitcoinAddress(keyID);
    // tokenKey = pwalletMain->GetBlindingPubKey(GetScriptForDestination(CTxDestination(keyID)));

    // // Attempt a send.
    // CWalletTx wtx;
    // SendGenerationTransaction(GetScriptForDestination(assetAddr.Get()), assetKey, GetScriptForDestination(tokenAddr.Get()), tokenKey, nAmount, -1, true, entropy, asset, reissuanceToken, wtx);

    // std::string blinds;
    // for (unsigned int i=0; i<wtx.tx->vout.size(); i++) {
    //     blinds += "blind:" + wtx.GetOutputBlindingFactor(i).ToString() + "\n";
    //     blinds += "assetblind:" + wtx.GetOutputAssetBlindingFactor(i).ToString() + "\n";
    // }

    UniValue obj(UniValue::VOBJ);
    // obj.push_back(Pair("txid", wtx.tx->GetHash().GetHex()));
    // for (uint64_t i = 0; i < wtx.tx->vin.size(); i++) {
    //     if (!wtx.tx->vin[i].assetIssuance.IsNull()) {
    //         obj.push_back(Pair("vin", i));
    //         blinds += "issuanceblind:" + wtx.GetIssuanceBlindingFactor(i, false).ToString() + "\n";
    //         break;
    //     }
    // }

    // AuditLogPrintf("%s : reissueasset txid:%s\nblinds:\n%s\n", getUser(), wtx.GetHash().GetHex(), blinds);

    return obj;
}

UniValue listissuances(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 0)
        // TODO NOKUBIT:
        throw std::runtime_error(
            "listissuances ( asset ) \n"
            "\nList all issuances known to the wallet for the given asset, or for all issued assets if none provided.\n"
            "\nArguments:\n"
            "1. \"asset\"                 (string, optional) The asset whose issaunces you wish to list.\n"
            "\nResult:\n"
            "[                     (json array of objects)\n"
            "  {\n"
            "    \"txid\":\"<txid>\",   (string) Transaction id for issuance.\n"
            "    \"entropy\":\"<entropy>\" (string) Entropy of the asset type.\n"
            "    \"asset\":\"<asset>\", (string) Asset type for issuance if known.\n"
            "    \"assetlabel\":\"<assetlabel>\", (string) Asset label for issuance if set.\n"
            "    \"token\":\"<token>\", (string) Token type for issuance.\n"
            "    \"vin\":\"n\",         (numeric) The input position of the issuance in the transaction.\n"
            "    \"assetamount\":\"X.XX\",     (numeric) The amount of asset issued. Is -1 if blinded and unknown to wallet.\n"
            "    \"tokenamount\":\"X.XX\",     (numeric) The reissuance token amount issued. Is -1 if blinded and unknown to wallet.\n"
            "    \"isreissuance\":\"<bool>\",  (bool) True if this is a reissuance.\n"
            "    \"assetblinds\":\"<blinder>\" (string) Hex blinding factor for asset amounts.\n"
            "    \"tokenblinds\":\"<blinder>\" (string) Hex blinding factor for token amounts.\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\"\"                 (array) List of transaction issuances and information in wallet\n"
            + HelpExampleCli("listissuances", "<asset>")
            + HelpExampleRpc("listissuances", "<asset>")
        );

    // RPCTypeCheck(request.params, boost::assign::list_of(UniValue::VSTR));

    // LOCK2(cs_main, pwalletMain->cs_wallet);

    // std::string assetstr;
    // CAsset assetfilter;
    // if (request.params.size() > 0) {
    //     assetstr = request.params[0].get_str();
    //     if (!IsHex(assetstr) || assetstr.size() != 64)
    //         throw JSONRPCError(RPC_TYPE_ERROR, "Asset must be a hex string of length 64");
    //     assetfilter.SetHex(assetstr);
    // }

    UniValue issuancelist(UniValue::VARR);
    // for (const auto& it : pwalletMain->mapWallet) {
    //     const CWalletTx* pcoin = &it.second;
    //     CAsset asset;
    //     CAsset token;
    //     uint256 entropy;
    //     for (uint64_t vinIndex = 0; vinIndex < pcoin->tx->vin.size(); vinIndex++) {
    //         UniValue item(UniValue::VOBJ);
    //         const CAssetIssuance& issuance = pcoin->tx->vin[vinIndex].assetIssuance;
    //         if (issuance.IsNull()) {
    //             continue;
    //         }
    //         if (issuance.assetBlindingNonce.IsNull()) {
    //             GenerateAssetEntropy(entropy, pcoin->tx->vin[vinIndex].prevout, issuance.assetEntropy);
    //             CalculateAsset(asset, entropy);
    //             // Null is considered explicit
    //             CalculateReissuanceToken(token, entropy, issuance.nAmount.IsCommitment());
    //             item.push_back(Pair("isreissuance", false));
    //             item.push_back(Pair("token", token.GetHex()));
    //             CAmount itamount = pcoin->GetIssuanceAmount(vinIndex, true);
    //             item.push_back(Pair("tokenamount", (itamount == -1 ) ? -1 : ValueFromAmount(itamount)));
    //             item.push_back(Pair("tokenblinds", pcoin->GetIssuanceBlindingFactor(vinIndex, true).GetHex()));
    //             item.push_back(Pair("entropy", entropy.GetHex()));
    //         } else {
    //             CalculateAsset(asset, issuance.assetEntropy);
    //             item.push_back(Pair("isreissuance", true));
    //             item.push_back(Pair("entropy", issuance.assetEntropy.GetHex()));
    //         }
    //         item.push_back(Pair("txid", pcoin->tx->GetHash().GetHex()));
    //         item.push_back(Pair("vin", vinIndex));
    //         item.push_back(Pair("asset", asset.GetHex()));
    //         const std::string label = gAssetsDir.GetLabel(asset);
    //         if (label != "") {
    //             item.push_back(Pair("assetlabel", label));
    //         }
    //         CAmount iaamount = pcoin->GetIssuanceAmount(vinIndex, false);
    //         item.push_back(Pair("assetamount", (iaamount == -1 ) ? -1 : ValueFromAmount(iaamount)));
    //         item.push_back(Pair("assetblinds", pcoin->GetIssuanceBlindingFactor(vinIndex, false).GetHex()));
    //         if (!assetfilter.IsNull() && assetfilter != asset) {
    //             continue;
    //         }
    //         issuancelist.push_back(item);
    //     }
    // }
    return issuancelist;
}
