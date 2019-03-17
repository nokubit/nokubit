// TODO NOKUBIT: header




#ifndef BITCOIN_NOKUBIT_CONFIG_H
#define BITCOIN_NOKUBIT_CONFIG_H

#include <nokubit/asset.h>


//  src/chainparamsbase.cpp
//#define MAINNET_NAME                        "main"     BITCOIN
#define MAINNET_NAME                        "nokubit"

//#define MAINNET_DIR                         ""     BITCOIN
#define MAINNET_DIR                         "nokubit"
//#define CONF_DEFAULT_RPC_PORT               8332     BITCOIN
#define CONF_DEFAULT_RPC_PORT               8267

//#define CONF_DEFAULT_TEST_RPC_PORT          18332     BITCOIN
#define CONF_DEFAULT_TEST_RPC_PORT          18267

//#define CONF_DEFAULT_REGT_RPC_PORT          18443     BITCOIN
#define CONF_DEFAULT_REGT_RPC_PORT          18267





//  src/chainparams.cpp
//#define CONF_GENESIS_EXTRANONCE             4     BITCOIN
//#define CONF_GENESIS_EXTRANONCE             347
#define CONF_GENESIS_EXTRANONCE             15786

//#define CONF_GENESIS_NONCE                  2083236893     BITCOIN
//#define CONF_GENESIS_NONCE                  1157102918
#define CONF_GENESIS_NONCE                  3949718613

//#define CONF_GENESIS_HASH                  "0x000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f"     BITCOIN
//#define CONF_GENESIS_HASH                  "0x00000000000b3eeaf0854cb5ee933074de5055a819421ca48114bd74dee4afb4"
#define CONF_GENESIS_HASH                  "0x000000000002372343785ee8ff3cf5af1638edd078d19c1d03008cf9aadae0da"

//#define CONF_GENESIS_MERKLE                "0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"
//#define CONF_GENESIS_MERKLE                "0xaa99046356402fb3425c91731a44f6bb650f39dcba54ba4aec3be89d792ea025"
#define CONF_GENESIS_MERKLE                "0xeffac3dbabdc98d0f8ec9cb06d5c4b47b7b25933b87a6e1809126ccdce0104f4"

//#define CONF_MINIMUM_CHAINWORK              "0x000000000000000000000000000000000000000000f91c579d57cad4bc5278cc"     BITCOIN
// TODO NOKUBIT: Da decidere in fase di test, poi e con PoS attivato
#define CONF_MINIMUM_CHAINWORK              "0x0000000000000000000000000000000000000000000000000000000100010000"

//#define CONF_ASSUMEVALID                    "0x0000000000000000005214481d2d96f898e3d5416e43359c145944a909d242e0" //506067     BITCOIN
#define CONF_ASSUMEVALID                    "0x00"

#define CONF_PK_NOKUBIT                     "0298bbdc27373a3c9383df6b223fbad062fac43ceae2c76a368b4b0e20ffb36aff"
#define CONF_POA_0                          "64c63389ce47df1c11ad9df7d3c7d3881749e710"

//#define CONF_MESSAGESTART_0                 0xf9     BITCOIN
//#define CONF_MESSAGESTART_1                 0xbe
//#define CONF_MESSAGESTART_2                 0xb4
//#define CONF_MESSAGESTART_3                 0xd9
#define CONF_MESSAGESTART_0                 0xf9
#define CONF_MESSAGESTART_1                 0xbe
#define CONF_MESSAGESTART_2                 0xb4
#define CONF_MESSAGESTART_3                 0xd9

//#define CONF_DEFAULT_P2P_PORT               8333     BITCOIN
#define CONF_DEFAULT_P2P_PORT               8270

//#define CONF_BECH32_HRP                     "bc"     BITCOIN
#define CONF_BECH32_HRP                     "nkb"


//#define CONF_GENESIS_TEST_NONCE             414098458     BITCOIN
#define CONF_GENESIS_TEST_NONCE             65377760

//#define CONF_GENESIS_TEST_HASH              "0x000000000933ea01ad0ee984209779baaec3ced90fa3f408719526f8d77f4943"     BITCOIN
#define CONF_GENESIS_TEST_HASH              "0x00000000d8295f0e4d73be942c6ef05996df5ddb31fffedb183a6bacd384a47b"

//#define CONF_MINIMUM_TEST_CHAINWORK         "0x00000000000000000000000000000000000000000000002830dab7f76dbb7d63"     BITCOIN
// TODO NOKUBIT: Da decidere in fase di test, poi e con PoS attivato
#define CONF_MINIMUM_TEST_CHAINWORK              "0x00000000000000000000000000000000000000000000002830dab7f76dbb7d63"

//#define CONF_TEST_ASSUMEVALID               "0x0000000002e9e7b00e1f6dc5123a04aad68dd0f0968d8c7aa45f6640795c37b1" //1135275     BITCOIN
#define CONF_TEST_ASSUMEVALID               "0x00"

#define CONF_PK_TEST_NOKUBIT                "0298bbdc27373a3c9383df6b223fbad062fac43ceae2c76a368b4b0e20ffb36aff"
#define CONF_POA_TEST_0                     "64c63389ce47df1c11ad9df7d3c7d3881749e710"

//#define CONF_DEFAULT_P2PTEST_PORT           18333     BITCOIN
#define CONF_DEFAULT_P2PTEST_PORT           18270


//#define CONF_GENESIS_REGT_NONCE             2     BITCOIN
#define CONF_GENESIS_REGT_NONCE             64

//#define CONF_GENESIS_REGT_HASH              "0x0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206"     BITCOIN
#define CONF_GENESIS_REGT_HASH              "0x0f777bcc3cb59d0efded7b13711e905ed240662e219c7f2f9e3b0a2c1e035e62"

#define CONF_PK_REGT_NOKUBIT                "0298bbdc27373a3c9383df6b223fbad062fac43ceae2c76a368b4b0e20ffb36aff"
#define CONF_POA_REGT_0                     "64c63389ce47df1c11ad9df7d3c7d3881749e710"

//#define CONF_DEFAULT_P2PREGT_PORT           18444     BITCOIN
#define CONF_DEFAULT_P2PREGT_PORT           18381





//  src/validation.h
//#define CONF_DEFAULT_MIN_RELAY_TX_FEE       1000     BITCOIN
#define CONF_DEFAULT_MIN_RELAY_TX_FEE       1000
#define CONF_PEGIN_MIN_RELAY_TX_FEE         1000
#define CONF_ORGANIZATION_MIN_RELAY_TX_FEE  1000
#define CONF_MINTASSET_MIN_RELAY_TX_FEE     1000
#define CONF_ASSETTX_MIN_RELAY_TX_FEE       1000




//  src/consensus/consensus.h
#define CONF_PEGIN_MATURITY                 100
#define CONF_ASSET_MATURITY                 100




//  src/policy/policy.h
#define CONF_DEFAULT_BLOCK_MIN_TX_FEE       1000




//  src/nokubit/rpcasset.cpp
#define CONF_DEFAULT_PEGIN_FEE              1000




//  src/nokubit/tagblock.h
// per test
//#define CONF_BLOCK_REWARD_PERIOD            10
#define CONF_BLOCK_REWARD_PERIOD            10000




//  src/validation.cpp
class CFeeRate;
class CTxMemPool;
class CBlockIndex;

CFeeRate GetMinFee(const CTxMemPool& pool, const CTransaction& tx, size_t sizelimit);
void getsuperblockversion(const CBlockIndex* pindexPrev, int32_t& nVersion);

//  src/consensus/tx_verify.cpp
bool getsharingfee(const CAmount& txfee, const asset_tag::CAmountMap value_out, CAmount& txsharingfee);

//  src/rpc/mining.cpp
class CBlock;
class UniValue;

void getsuperblockcaps(CBlock* pblock, UniValue& aCaps);
void getsuperblocktemplate(CBlock* pblock, UniValue& result);

#endif  // BITCOIN_NOKUBIT_CONFIG_H
