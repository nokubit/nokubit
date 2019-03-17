# Nokubit sidechain started from bitcoin-0.15.1

Sviluppo del progetto e prove effettuate

# CAPITOLO 1

Il progetto è clonato dal progetto Bitcoin TAG bitcoin-0.15.1

Creata la directory nokubit

    git init

Unzippato il progetto bitcoin

    git add *
    git commit ...

Compilazione seguendo le indicazioni contenute in [doc/build-unix.md](doc/build-unix.md)

* verifica e installazione (già presente quasi tutto)

    sudo apt-get install build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils

* installazione librerie Boost

    sudo apt-get install libboost-system-dev libboost-filesystem-dev libboost-chrono-dev libboost-program-options-dev libboost-test-dev libboost-thread-dev

* Berkeley DB. Non mi è chiaro decido di saltare.


Prendendo esempio dalle istruzioni del paragrafo 'Setup and Build Example: Arch Linux'
eseguo il build di autotools ed il make (nokubit è la directory del progetto locale)

    cd nokubit/
    ./autogen.sh
    ./configure --disable-wallet --without-gui --without-miniupnpc
    make check

la compilazione e l'esecuzione dei test si conclude in meno di 20 minuti.

## Prima constatazione

Il gitignore del progetto è fatto bene, dopo la compilazione un git status NON riporta
modifiche o file aggiunti (**NB autotools**)

Esportazione su gitlab


Prove di esecuzione
NB lo scopo è fare una sidechain non un nodo bitcoin quindi tutti i comandi avranno l'argomento **-regtest**

Preparo la directory .bitcoin nella mia home

    cd .bitcoin
    cp ../nokubit/contrib/debian/examples/bitcoin.conf .
    chmod 0600 bitcoin.conf

personalizzazione del file

    regtest=1
    rpcuser=nokubit
    rpcpassword=....

esecuzione del servizio

    ../nokubit/src/bitcoind -regtest -daemon

Parte e rilascia il prompt (argomento -daemon)

il servizio crea la directory regtest e la riempe di roba

**NB per fermare il servizio basta un kill**

LOG

    more regtest/debug.log

*Sembra* funzionare bene.

## Prove

Decido di seguire i [develope example](https://bitcoin.org/en/developer-examples#regtest-mode)
Genero i blocchi per incassare BEN 5050 nokubitcoin e poterne spendere 50

    ../nokubit/src/bitcoin-cli -regtest generate 101

**ERRORE** tradotto: comando non implementato.

La scelta di non compilare il wallet ha colpito: tutti i comandi dell'esempio sono proprio li.

Berkeley DB: scelgo la modalità più veloce, installo la versione corrente e compilo con wallet incompatibile.
**Se qualche anima pia vuole provare la versione compatibile è ben accetta.**

quindi

    sudo apt-get install libdb-dev libdb++-dev
    cd nokubit/
    ./autogen.sh
    ./configure --without-gui --without-miniupnpc --with-incompatible-bdb
    make
    cd ..
    ./nokubit/src/bitcoind -regtest -daemon -debug=1

Eseguiti i test dell'esempio fino a *Complex Raw Transaction* escluso.

Non riporto i dettagli dei test. Lo scopo è stato verificare che la compilazione ha funzionato correttamente e quindi abbiamo una base di partenza *certa*.


# CAPITOLO 2

Isolata la chain, eliminati i seed e dnsSeed. Ridotto PoW. Eventualmente da riabilitare con i dati opportuni quando ci saranno più nodi.


# CAPITOLO 3

Il progetto è stato riallineato alla versione 0.16 di Bitcoin (RC2).

Riapplicate le modifiche precedenti, ampliate le modifiche ai parametri del consensus.
La main chain è in grado di minare, indipendentemente dai limiti di PoW, un blocco al minuto.
Ovvero per minare un blocco occorre aspettare un minuto oppure eseguire il comando con un numero di try pù alto:
    bitcoin-cli generate 101 100000000
il comando non mina 101 blocchi, ma circa una decina. Purtroppo un try più alto incoccia nel timeout di connessione.


# CAPITOLO 4

### Sviluppo PegIn

Modificato consensus SegWit, il controllo viene fatto ogni 1916 blocchi, quindi alla partenza si casca nella coinbase: per la coinbase l'attivazione di un'opzione è THRESHOLD_DEFINED, non THRESHOLD_ACTIVE.
Valutato l'uso dell'opzione '-prematurewitness', ma non è utilizzata in tutti i casi, quindi, per adesso, sono state forzate le BIP9 a ALWAYS_ACTIVE.

Aggiunta la directory nokubit, contiene tutto il codice isolabile necessario per la modifica di bitcoin.
Con lo scopo di ridurre al minimo le modifiche ai file esistenti:

* chainparambase.cpp

    La mainchain viene costruita nella sottodirectory nokubit.

* chainparam consensus/params.h

    Modifiche già viste, manca da definire una nuova genesi.

    Chiave pubblica per la creazione del PegIn, da valutare la sicurezza di questa soluzione. Questa obbliga all'utilizzo ripetuto di una sola chiave privata. *Valutare l'uso di chiavi HD*.

* util

    Aggiunta opzione di debug BCLog::ASSET

* validation.cpp

    Modifica forzatura BIP30 e BIP34 (prev vers)

    Eliminato il sussidio in GetBlockSubsidy

    In presenza di Tag nella transazione, il Tag viene esplicitato e validato in CheckBlock

* consensus/tx_verify.h

    In GetTransactionSigOpCost nel conto del PegIn è stata aggiunta una operazione di firma

    In CheckTxInputs eseguita la verifica della struttura del PegIn

* policy/policy

    Aggiunto MAX_STANDARD_P2WSH_STACK_TAG_SIZE: valore di massima dimensione Witness per i Tag in IsWitnessStandard

* primitives/transaction

    Gestione Tag in TxIn

* rpc/client.cpp rpc/rawtransaction.cpp

    Aggiunti riferimenti al comando createrawpegin

### Descrizione Tag e PegIn

#### Premessa

Il Tag (asset_tag::AssetTag) ha una interdipendenza con le classi CTransaction & Co
inoltre il team bitcoin sta cercando di separare le funzionalità del progetto in librerie distinte e indipendenti, questo comporta che il Tag deve essere compatibile con la libreria bitcoinconsensus.
Di conseguenza non può utilizzare funzionalità, per esempio la firma, non comprese nella libreria. In questa fase dello sviluppo il Tag è quindi strutturato in due livelli (PIMPL Pointer to IMPLementation o Qt D-Pointer), nella definizione di CTxIn il puntatore è di tipo AssetTag in AssetTag il puntatore è di tipo ScriptTag.
In questo modo si elimina al primo livello l'interdipendenza e al secondo livello le limitazioni imposte da libbitcoinconsensus. *Da verificare se il doppio livello è vermanete necessario*.

#### Struttura

Ogni Tag è una (o sarà una parte di una) transazione nokubit. In particolare la prima (per adesso solo la prima) TxIn trasporta le informazioni del Tag nel primo elemento dello stack della sua Witness.
In memoria il Tag viene esplicitato nella classe **AssetTag/ScriptTag** puntata dal campo **tagAsset** della CTxIn. Per rendere più efficiente e certa la sua serializzazione è stata utilizzata una modifica nella classe COutPoint (campo prevout di CTxIn): il bit più significativo del campo **n** di COutPoint è utilizzato come flag di presenza Tag nella e solo nella serializzazione della transazione. Questa tecnica è presa dal progetto Elements-Alpha (nei file transaction e asset sono presenti altri pezzetti di codice presi da quel progetto; il progetto nokubit adesso ha preso una strada diversa e indipendente, ma questi pezzetti sono ancora presenti come memorandum per lo sviluppo del Tag di tipo Asset).

Nella fase di serializzazione, se è presente un Tag, l'indice della transazione spesa (**n** di prevout) viene flaggato con 0x80000000 (0 => 0x80000000, 1 => 0x80000001 ecc). In fase di deserializzazione se **n** ha il bit più significativo settato e non è una coinbase, viene inizializzato il campo **tagAsset** ed il valore di **n** ripulito dal flag.

NB: Il bit di flag è gestito nella classe CTxIn e non in COutPoint. Viene esplicitato solo nella serializzazione
eseguita da CTxIn. Le serializzazioni eseguite direttamente dalla clasee COutPoint non esplicitano il flag.


#### Classe ScriptTag

E' la classe da cui derivano tutti i Tag, ovvero la struttura interna dei Tag.
Ogni Tag è serializzato nel primo elemento dello stack della Witness della TxIn a cui appartiene.
La struttura base è costituita da un prefisso e da un carattere che definisce il tipo.
Il prefisso è sempre la stringa **'nkb'**, il tipo è un carattere mnemonico che identifica il Tag, p.es. il PegIn è identificato dal carattere **'p'**.
Inoltre mantiene un flag (campo **tagValid**) che memorizza lo stato di Validità dello specifico Tag, ed espone i metodi virtuali necessari per la gestione dei vari Tag.

**Struttura PegIn** (classe PeginScript)

La struttura interna è la seguente (type 'p'):

    uint256 chainGenesis;
    uint32_t blockHeight;
    COutPoint chainTx;
    std::vector<unsigned char> signature;
    CAmount nValue;

Rappresentano i dati della transazione sulla mainchain (*Atomic Exchange*) che bloccano il valore **nValue** trasformato in **nokubit**. Il campo **signature** è la firma dei restanti campi, verificabile dalla chiave pubblica inserita nel consensus. (*Vedi nota sulla sicurezza*).

Il metodo IsValidate ed il campo tagValid verificano e riportano la correttezza della firma, garantendo che il PegIn è stato emesso da Noku.

**Struttura MainRef** (classe ReferenceScript)

La struttura interna è la seguente (type 'r'):

    COutPoint chainTx;
    AssetAmount nValue;
    CScript parentWitnessProgram;

Questa è una struttura ausiliaria che simula la TxOut della mainchain. Viene creata in parallelo al PegIn ed inserita come Coin nella cache **pcoinsTip** (Utxo già confermati).
Questa struttura si è resa necessaria in alternativa alla scelta di Elements-Alpha di modificare il codice in tutti i punti (e sono tanti) nei quali bitcoin verifica la correttezza di una transazione.

**Strutture da implementare** (previste)

Queste sono le strutture in questo momento previste e da implementare nel prossimo step:

    NkbOrganization = 'o',
    NkbMintAsset = 'm',
    NkbAsset = 'a',

La difficoltà principale da risolvere è la suddivisione o specializzazione delle Utxo in base alla moneta o Asset.

#### RPC/API rpcasset.cpp

Come viene inserito nella chain un Pegin? Il file rpcasset implementa un nuovo comando Rpc (**createrawpegin**) che accetta un PegIn, lo verifica e lo inserisce nelle Utxo da minare.
La transazione contenente il PegIn può arrivare, e in produzione arriverà ai nodi, tramite il colloquio internodale (comando TX). Questo punto è da verificare.

## Prove

Compilazione, configurazione e comandi base del test non cambiano.

Esecuzione del servizio (l'opzione debug è a piacere) e stop dello stesso

    bitcoind -daemon -listenonion=false -debug=asset
    bitcoin-cli stop

comandi utili

    bitcoin-cli help
    bitcoin-cli generate 1 40000000
    bitcoin-cli getblockcount
    bitcoin-cli getchaintips
    bitcoin-cli getblockchaininfo
    bitcoin-cli getnewaddress "" bech32
    bitcoin-cli getrawmempool
    bitcoin-cli getrawmempool true
    bitcoin-cli listtransactions
    bitcoin-cli listunspent
    bitcoin-cli listunspent 0
    bitcoin-cli getbalance
    bitcoin-cli sendtoaddress <bech32 address> <value>

comando per la generazione di un PegIn (chainGenesis è quella di Bitcoin)  
$NEW_ADDRESS è la variabile che contiene il destinatario  
$BITCOIN_TXID $BITCOIN_VOUT e chainGenesis rappresentano la transazione bloccata sulla mainchain  
$SIGNATURE deve essere la firma del PegIn con la chiave privata Noku, adesso è una chiave di test  

    bitcoin-cli createrawpegin '''
      {
        "chainGenesis": "000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f",
        "chainTxId": "'$BITCOIN_TXID'",
        "chainVOut": '$BITCOIN_VOUT',
        "signature" : "'$SIGNATURE'",
        "nValue": 10
      }
      ''' '''
      "'$NEW_ADDRESS'"
    
    bitcoin-cli generate 1 20000000


Il PegIn è visibile nella catena come valuta, anche prima di essere minato.  
Dopo essere minato anche nel balance del destinatario e puo essere speso.

NB Per la firma e la creazione del PegIn sto preparando un comando js nel progetto nokujs.

# CAPITOLO 5

Il progetto è stato riallineato alla versione finale 0.16.0 di Bitcoin.
Per un totale di 66 file modificati, dei quali 8 riguardanti il progetto. 

# CAPITOLO 6

Definizione della classe asset_tag::AssetAmount e tipi correlati, in sostituzione di CAmount.
Sostituzione in tutti i file della nuova classe (non in bench e test)

CAmount è un typedef di int64_t e rappresenta la valuta di Bitcoin in satoshi, p.es. in CTxOut.

La nuova classe ha lo scopo di aggiungere alla valuta di Bitcoin una stringa che, se non nulla identifica l'asset.

In questa fase il nome dell'asset verrà serializzato da CTxOut, modificando il formato delle transazioni e dei blocchi. In alternativa si potra utilizzare una Witness con versione != 0 che serializza il nome dell'asset e la nuova classe rimane solo in memoria.

La modifica non è stata riportata nei sorgenti di test e benchmark, quindi per il momento non si possono compilare.

# CAPITOLO 7

Allineamento sorgenti Benchmark e Test da CAmount a asset_tag::AssetAmount.
I file sono compilabili, ma poiché le transazioni sono diverse non tutti sono funzionanti.

Implementazione Stati (NkbOrganization) e creazione Asset.

**Struttura Stato** (classe OrganizationScript)

La struttura interna è la seguente (type 'o'):

    std::string organizationName;
    std::string organizationDescription;
    WitnessV0KeyHash organizationAddress;
    unsigned char enableFlag;
    std::vector<unsigned char> signature;

Questa è la struttura che consente di abilitare/disabilitare l'indirizzo di uno Stato. Come per il PegIn può essere generato solo da Noku: la firma viene verificata dalla chiave pubblica del consensus.
La transazione che porta questa struttura aggiunge/toglie un indirizzo dalla base dati delle reward per gli Stati (*da implementare*).

**Strutture creazione Asset** (MintAssetScript)

La struttura interna è la seguente (type 'm'):

    std::string assetName;                  // Id, Nome univoco
    std::string assetDescription;
    CPubKey assetAddress;                   // emittente
    WitnessV0KeyHash stateAddress;          // Stato (Opzionale e modificabile)
    unsigned char issuanceFlag;             // Flag: open to re-issuance
    unsigned char extraWitness;             // Per consentire future espansioni (coppie Nome:Valore)
    std::vector<unsigned char> signature;
    CAmount nAssetValue;
    // CAmount nValue;                     // Nokubit depositati
    // CAmount nFee;                       // Nokubit Fee emissione

In questa versione sono state unificate le strutture 'NkbMintAsset' e 'NkbAsset' precedentemente ipotizzate
Questa è la struttura che consente di definire un Asset e di ri/emettere una definita quantità di token.
A differenza del PegIn può essere generato liberamente, la firma viene verificata dalla chiave pubblica contenuta della stessa struttura.
Eventuali successive modifiche, se consentite, devono contenere la stessa chiave pubblica.
Le caratteristiche di un Asset sono mantenute in una base dati (*da implementare*).
La transazione che porta questa struttura crea valuta di tipo assetName.


# CAPITOLO 7

Test di creazione Stati e Asset, modifiche e adeguamenti al codice.
La *chain* generata da questa versione è **incompatibile** con le precedenti.

Modifiche:

* chainparam.cpp

    Definire una nuova genesi, manca la versione definitiva (formato finale e supporto HW).

* core_io.h rpc/server

    Formato JSON per gli asset: {"nomeAsset": valore}
    Internamente il valore è rappresentato in satoshi, quindi non ha decimali.

* consensus/tx_verify.cpp

    Estensione verifiche sui nuovi tag

* nokubit/*

    Aggiunte modifiche, riorganizzazione
    RPC per i nuovi Tag

* rpc/client.cpp rpc/rawtransaction.cpp

    definizioni per i nuovi RPC

* primitives/transaction.h

    Adeguamento ai nuovi Tag

Nel progetto nokujs sono stati aggiunti/modificati degli script per generare i nuovi Tag.

## Esempio di scambio Asset

Le transazioni utilizzate sono state precedentemente create con nbAsset.js

    Noku/nokubit/src/bitcoin-cli getrawtransaction c2b2f9229da26a532449283d933d758bfc8a38322aacceed6bb41c9ce593a668 true

    {
    "txid": "c2b2f9229da26a532449283d933d758bfc8a38322aacceed6bb41c9ce593a668",
    "hash": "8102817a76b3e62d51581779928d6fd76bbba7b9c21f222d53e8306135e564a5",
    "version": 2,
    "size": 224,
    "vsize": 143,
    "locktime": 0,
    "vin": [
        {
        "txid": "76751b1fe56601fb80d430e88406e06a8635f18c485f453943d168910207f4f9",
        "vout": 0,
        "scriptSig": {
            "asm": "",
            "hex": ""
        },
        "txinwitness": [
            "30440220459c53da3f177e356933fbd7beee93d310a68fa2f8b5c5868933b7b77d39f203022075d9a6b3fb32cdacd7d07fe350eca838f3b9a74020c88bf9c3c2851cf2c24e5001",
            "025e6bae2c12f85e9ce267ea6f998d8de0748e64a89e04d060fc7ec4f2e524e6b0"
        ],
        "sequence": 4294967294
        }
    ],
    "vout": [
        {
        "value": 54.99996140,
        "n": 0,
        "scriptPubKey": {
            "asm": "0 f4b927fd052d1bcd52abc0014a28f7caf965b327",
            "hex": "0014f4b927fd052d1bcd52abc0014a28f7caf965b327",
            "reqSigs": 1,
            "type": "witness_v0_keyhash",
            "addresses": [
            "bc1q7juj0lg995du654tcqq5528hetuktve8tjxpla"
            ]
        }
        },
        {
        "value": 100.00000000,
        "n": 1,
        "scriptPubKey": {
            "asm": "0 c2705b2b868e77b2a8cdfa3fbdc034d4cc833ae3",
            "hex": "0014c2705b2b868e77b2a8cdfa3fbdc034d4cc833ae3",
            "reqSigs": 1,
            "type": "witness_v0_keyhash",
            "addresses": [
            "bc1qcfc9k2ux3emm92xdlglmmsp56nxgxwhrckccn3"
            ]
        }
        }
    ],
    "hex": "02000000000101f9f407029168d14339455f488cf135866ae00684e830d480fb0166e51f1b75760000000000feffffff0200ec47d34701000000160014f4b927fd052d1bcd52abc0014a28f7caf965b3270000e40b5402000000160014c2705b2b868e77b2a8cdfa3fbdc034d4cc833ae3024730440220459c53da3f177e356933fbd7beee93d310a68fa2f8b5c5868933b7b77d39f203022075d9a6b3fb32cdacd7d07fe350eca838f3b9a74020c88bf9c3c2851cf2c24e500121025e6bae2c12f85e9ce267ea6f998d8de0748e64a89e04d060fc7ec4f2e524e6b000000000",
    "blockhash": "000000ff46de8eea1be7b9797b54b197a1066529cccb808cca21614193cb1ed6",
    "confirmations": 1,
    "time": 1529565579,
    "blocktime": 1529565579
    }

```
    Noku/nokubit/src/bitcoin-cli getrawtransaction 7b135b642b983aab400c7a649e52ec725c0cd932a172cb64df90f3ec16cd5124 true

    {
    "txid": "7b135b642b983aab400c7a649e52ec725c0cd932a172cb64df90f3ec16cd5124",
    "hash": "c7736df7c750501688590560daf6ea049c891d91d320cca6196113156952781b",
    "version": 2,
    "size": 470,
    "vsize": 242,
    "locktime": 0,
    "vin": [
        {
        "txid": "c53e06dedcab852057a8bcd5baa74398923492959e5de7f3299a30c70c069744",
        "vout": 0,
        "scriptSig": {
            "asm": "",
            "hex": ""
        },
        "txinwitness": [
            "6e6b626d094e6f6b7550656e6e79134173736574204e6f6b752064692070726f76612102dac1a06a52355a65baa8529924f568b15f0c2039f1068ee31082de62b821b9890000000000000000000000000000000000000000a06d0e02000000000000463044022076854984fa3ebdec254cc18b658c1c9676b535b89eb3da6a435b95f4d4e4003302200980bbd1a5bbbd322a46ab1c77ffd682b358da572fe63187c56a005c122226d3",
            "a9147b4ef67be39220fbc2ef240fe32cbcff90b1296b87"
        ],
        "sequence": 4294967295
        },
        {
        "txid": "e7c75001de400780c0f7d96ae9d0cd74f1037a766daa55b0f80a7876197599ac",
        "vout": 0,
        "scriptSig": {
            "asm": "",
            "hex": ""
        },
        "txinwitness": [
            "3045022100cceb7e2c06e87a7b36a2f8a33b8e47b19ba4fa3f807ce298dbb1cf28875610a6022059f87d16f3157b82d63dce5475e1bd38aa01cc7c669a83fe96af4639de60e39701",
            "03f86dff3f03ac56d7264566248b571a70b4cdbb53bc296af8bd0b69cc7701d95e"
        ],
        "sequence": 4294967295
        }
    ],
    "vout": [
        {
        "value": {
            "NokuPenny": "34500000"
        },
        "n": 0,
        "scriptPubKey": {
            "asm": "0 c2705b2b868e77b2a8cdfa3fbdc034d4cc833ae3",
            "hex": "0014c2705b2b868e77b2a8cdfa3fbdc034d4cc833ae3",
            "reqSigs": 1,
            "type": "witness_v0_keyhash",
            "addresses": [
            "bc1qcfc9k2ux3emm92xdlglmmsp56nxgxwhrckccn3"
            ]
        }
        },
        {
        "value": 0.31140000,
        "n": 1,
        "scriptPubKey": {
            "asm": "0 ce93c82cce5f8b5b6e337efe19127c8d39402702",
            "hex": "0014ce93c82cce5f8b5b6e337efe19127c8d39402702",
            "reqSigs": 1,
            "type": "witness_v0_keyhash",
            "addresses": [
            "bc1qe6fustxwt794km3n0mlpjynu35u5qfczm4sac5"
            ]
        }
        }
    ],
    "hex": "020000000001024497060cc7309a29f3e75d9e959234929843a7bad5bca8572085abdcde063ec50000008000ffffffffac99751976780af8b055aa6d767a03f174cdd0e96ad9f7c0800740de0150c7e70000000000ffffffff02094e6f6b7550656e6e79a06d0e0200000000160014c2705b2b868e77b2a8cdfa3fbdc034d4cc833ae300a028db0100000000160014ce93c82cce5f8b5b6e337efe19127c8d3940270202a96e6b626d094e6f6b7550656e6e79134173736574204e6f6b752064692070726f76612102dac1a06a52355a65baa8529924f568b15f0c2039f1068ee31082de62b821b9890000000000000000000000000000000000000000a06d0e02000000000000463044022076854984fa3ebdec254cc18b658c1c9676b535b89eb3da6a435b95f4d4e4003302200980bbd1a5bbbd322a46ab1c77ffd682b358da572fe63187c56a005c122226d317a9147b4ef67be39220fbc2ef240fe32cbcff90b1296b8702483045022100cceb7e2c06e87a7b36a2f8a33b8e47b19ba4fa3f807ce298dbb1cf28875610a6022059f87d16f3157b82d63dce5475e1bd38aa01cc7c669a83fe96af4639de60e397012103f86dff3f03ac56d7264566248b571a70b4cdbb53bc296af8bd0b69cc7701d95e00000000",
    "blockhash": "000001df8ee39e713554071d4775fa7ce95370c7d2100a53eefe7ca9ce71527a",
    "confirmations": 3,
    "time": 1529524073,
    "blocktime": 1529524073
    }

```
    Noku/nokubit/src/bitcoin-cli createrawtransaction '''
    [{
        "txid": "c2b2f9229da26a532449283d933d758bfc8a38322aacceed6bb41c9ce593a668", "vout": 1
    },{
        "txid": "7b135b642b983aab400c7a649e52ec725c0cd932a172cb64df90f3ec16cd5124", "vout": 0
    }]
    ''' '''
    {
        "bc1qcfc9k2ux3emm92xdlglmmsp56nxgxwhrckccn3": [
            99.999,
            {"NokuPenny": 20000000}
        ],
        "bc1q28f28ru5vlp0kr6zdfwp4vt6mnp2mgp9q5hlqd": {"NokuPenny": 14500000}
    }
    '''

    =>
    020000000268a693e59c1cb46bedceac2a32388afc8b753d933d284924536aa29d22f9b2c20100000000ffffffff2451cd16ecf390df64cb72a132d90c5c72ec529e647a0c40ab3a982b645b137b0000000000ffffffff0300605d0a5402000000160014c2705b2b868e77b2a8cdfa3fbdc034d4cc833ae3094e6f6b7550656e6e79002d310100000000160014c2705b2b868e77b2a8cdfa3fbdc034d4cc833ae3094e6f6b7550656e6e79a040dd000000000016001451d2a38f9467c2fb0f426a5c1ab17adcc2ada02500000000

```
    Noku/nokubit/src/bitcoin-cli signrawtransaction "020000000268a693e59c1cb46bedceac2a32388afc8b753d933d284924536aa29d22f9b2c20100000000ffffffff2451cd16ecf390df64cb72a132d90c5c72ec529e647a0c40ab3a982b645b137b0000000000ffffffff0300605d0a5402000000160014c2705b2b868e77b2a8cdfa3fbdc034d4cc833ae3094e6f6b7550656e6e79002d310100000000160014c2705b2b868e77b2a8cdfa3fbdc034d4cc833ae3094e6f6b7550656e6e79a040dd000000000016001451d2a38f9467c2fb0f426a5c1ab17adcc2ada02500000000" '[]' '["L2GzngABTQMCavay7p192zs5i8EyYPFkJ4KXqBdgdw6aXv9dkncp"]' "ALL"

    =>
    {
    "hex": "0200000000010268a693e59c1cb46bedceac2a32388afc8b753d933d284924536aa29d22f9b2c20100000000ffffffff2451cd16ecf390df64cb72a132d90c5c72ec529e647a0c40ab3a982b645b137b0000000000ffffffff0300605d0a5402000000160014c2705b2b868e77b2a8cdfa3fbdc034d4cc833ae3094e6f6b7550656e6e79002d310100000000160014c2705b2b868e77b2a8cdfa3fbdc034d4cc833ae3094e6f6b7550656e6e79a040dd000000000016001451d2a38f9467c2fb0f426a5c1ab17adcc2ada02502483045022100d5b1930416ddd1af5257778a2bc6f72b1dad8f3dbf45c80d61b49ff481bd32bf0220138c834cf3623e53ab58b11509668fb46f548ed8d05aef74283f02928c502fbc012102fc1ef47101a9cbc8431c3b5e6fcc2ec0f72b4d5264fcaf0db39b0dc3b221c0c50247304402204036d88eee73c07c185fc8b3157e7c155ae33b88ceecb5b58be93b370ccfd66502205c1a1470142c13dccb9f6fbaf0a9669c4878c3fc6b6b8b1a55500753efabbe46012102fc1ef47101a9cbc8431c3b5e6fcc2ec0f72b4d5264fcaf0db39b0dc3b221c0c500000000",
    "complete": true
    }

```
    Noku/nokubit/src/bitcoin-cli sendrawtransaction "0200000000010268a693e59c1cb46bedceac2a32388afc8b753d933d284924536aa29d22f9b2c20100000000ffffffff2451cd16ecf390df64cb72a132d90c5c72ec529e647a0c40ab3a982b645b137b0000000000ffffffff0300605d0a5402000000160014c2705b2b868e77b2a8cdfa3fbdc034d4cc833ae3094e6f6b7550656e6e79002d310100000000160014c2705b2b868e77b2a8cdfa3fbdc034d4cc833ae3094e6f6b7550656e6e79a040dd000000000016001451d2a38f9467c2fb0f426a5c1ab17adcc2ada02502483045022100d5b1930416ddd1af5257778a2bc6f72b1dad8f3dbf45c80d61b49ff481bd32bf0220138c834cf3623e53ab58b11509668fb46f548ed8d05aef74283f02928c502fbc012102fc1ef47101a9cbc8431c3b5e6fcc2ec0f72b4d5264fcaf0db39b0dc3b221c0c50247304402204036d88eee73c07c185fc8b3157e7c155ae33b88ceecb5b58be93b370ccfd66502205c1a1470142c13dccb9f6fbaf0a9669c4878c3fc6b6b8b1a55500753efabbe46012102fc1ef47101a9cbc8431c3b5e6fcc2ec0f72b4d5264fcaf0db39b0dc3b221c0c500000000"

    =>
    d6ce57c390fce43775984aeff8c91e27b8db76aea20ec8e0e428f416d761b72d


# CAPITOLO 8

Verifica comunicazione internodale

Aggiustato consensus.nMinimumChainWork ai valori correnti (è molto basso).

Aggiunti a consensus.h ma non ancora utilizzati
    PEGIN_MATURITY
    ASSET_MATURITY

Spostato check formato tag da CheckBlock e CheckTxInputs in CheckTransaction con logica in tagscript.cpp

Spostato check firma tag 'IsValidate' da CheckBlock in CheckTxInputs con logica già in tagscript.cpp
Mettere qui controllo maturità Pegin e Asset.

Inserito in AcceptToMemoryPoolWorker e CChainState::ConnectBlock il prevout (ReferenceScript).

Verificato con successo ricezione di nuovi blocchi, nuove transazioni e start/stop del nodo.


# CAPITOLO 9

Modifica strutture Coin e MemPool

Aggiunta gestione Tag e maturity nella chainstate (Coin) e nella mempool

Controllo sovrascrittura TAG


# CAPITOLO 10

Fase di test con numero di nodi variabile

Test di start e restart della blockchain


# CAPITOLO 11

Revisione librerie javascript, file di esempio e di test


btAddrs.js
*   (new) prove firma EC via libreria interna NodeJs, conversione chiavi DER/RAW, con test su dati bitcoin
*   indirizzi bech32, codifica e decodifica + test presi dall BIP corrispondente.
*   indirizzi base58, codifica e decodifica tradotte dal codice Bitcoin + test con i  vettori che stanno nel sorgente bitcoin.
*   prove di prefisso su indirizze BIP32 (da non confondere con i Bech32)
*   prove di uso della libreria Node crypto per la ECDSA. (in alternativa lib npm 'elliptic')


nbPegin.js
*   test funzionalita  API blockchain.info
*   test accesso RPC/API nokubit (incompleto)


bthash.js
*   Prime prove su uso hash in Bitcoin
*   simulazione sminamento di un blocco.


convert.js
*   prova uso api (libere) coinbase


explorer.js
*   decodifica file blocchi bitcoin. Vuole come parametro la directory dei blocchi
        -dir=path_blocchi &nbsp; default = **./.bitcoin/regtest/blocks**


miner.js
*   implementazione del miner POA


nkSendTx.js
*   gestione invio transazioni


nbAsset.js
*   gestione asset


nbState.js
*   gestione stati


nokubit.js
*   command line interface


testSolver.js
*   test rpc resolver interno al nodo


testTx.js
*   test invio transazioni


# CAPITOLO 12

Rebase a Bitcoin TAG bitcoin-0.16.3

Sviluppo PoA compatibile con PoS/PoW ibrido.

Modifica Tag per eliminare rischi di malleabilità.


#### Premessa

Bitcoin è fortemente PoW. Quando un nodo riceve un nuovo blocco o una nuova header di un blocco, la prima operazione che esegue è verificare che
l'header, l'hash dell'header, rispetti il work in essa definito (nBits).

Nel PoS ibrido l'hash del blocco non rispetta questa verifica, altrimenti il PoS sarebbe inutile.

La soluzione implementata è basata su due ipotesi:
*   non modificare l'header del blocco
*   non eliminare il test PoW iniziale (rischio forte di DoS)

L'header del blocco non è stata modificata, ma è stato necessario inserire la prova PoS nella classe che rappresenta l'header (**CBlockHeader**), con conseguente modifica della classe **CBlockIndex** che rappresenta l'header nella chain e nel P2P.
Come conseguenza sono stati modificati il db **index** che adesso memorizza header e prova PoS, e in forma analoga il protocollo P2P, i comandi relativi alle header adesso trasportano anche la prova PoS.

#### Struttura

La prova PoS è stata definita in modo simile alla commitment Witness, utilizzando un nuovo Tag.
La coinbase di un blocco PoS contiene una TxOut con scriptPubKey definita come OP_RETURN e la push della **PoSScript** (il nuovo tag) serializzata.
La firma va in aggiunta alla *witness reserved value* dello stack witness.
La firma è definita come firma dell'dsha256 della prova PoS e dell'hash del blocco, da parte della chiave privata del proprietario dello stack. 

In memoria il Tag viene esplicitato nella classe **PosTag/PoSScript** puntata dal campo **tagPoS** della CBlockHeader.

Vedi anche la premessa nella definizione dei Tag

#### Classe PoSScript

E' la classe, distinta ma derivata da ScriptTag, che rappresenta la prova PoS.
Ogni Tag è serializzato nella scriptPubKey di una TxOut della coinbase, preceduto da un OP_RETURN e dalla stringa **'nkbs'**.
Inoltre memorizza l'hash del blocco a cui appartiene.

La struttura interna è la seguente:

    COutPoint stack;
    CAmount nStackValue;
    CPubKey stackAddres;
    std::vector<unsigned char> signature;

I campi **stack** e **nStackValue** rappresentano la uTxo bloccata per il PoS e la frazione di valore utilizzato, la uTxo verrà bloccata, resa non spendibile, in modo conforme alle definizioni di Michele. Il campo **stackAddress** è la chiave pubblica del proprietario dello stack.
Se il suo address a 20 byte è nell'elenco PoA, il campo **stack** non è utilizzato (hash 0, vout -1). Il campo **signature** è la firma dei restanti campi più l'hash del blocco.

Il metodo IsValidate ed il campo posValid verificano e riportano la correttezza della firma, garantendo che il blocco PoS rappresenta no stack valido e riporta una firma corretta.
Se l'istanza della classe è valida, sostituiscono l'hash dell'header del blocco con l'hash PoS del blocco.

La definizione di Hash PoS è temporanea.
Attualmente è calcolata semplicente come hash / stack.


* nokubit/tagscript

    Definizione classe **PoSScript** con definizione del metodo *GetPoSHash* e relativo calcolo in *ValidateSign*

    Indirettamente il metodo *GetPoSHash* sostituisce il metodo originale *GetHash*

    Aggiunto *destinationAddress* alle classi **PeginScript** e **MintAssetScript**.

    Gestione della verifica del destinatario

* nokubit/asset

    Definizione classe **PoSTag** con definizione del metodo *GetPoSHash*

    Il metodo *GetPoSHash* sostituisce il metodo originale *GetHash* nella classe **CBlockHeader** modificando la verifica PoW in PoS

* nokubit/rpcasset.cpp

    Adeguamento alle modifiche anti malleabilità.

* chainparam.cpp consensus/params.h

    Alzato leggermente nMinimumChainWork, definita una nuova genesi 46 bit.
    **Devono ancora essere definiti i parametri finali**.

    Definiti i DNS Seed delle macchine di prova.

    Definito vettore di *id* di Chiave (address 20 byte) abilitati al PoA.
    *Da aggiungere e gestire data inizio e fine*

* consensus/consensus.h

    Fissati nuovi valori dimensione blocco a 8MB, compatibili SegWit.

* protocol.h

    Aggiunto NODE_POS ai ServiceFlags.

    Definisce un nodo come PoS (potrebbe non servire)

* init.cpp

    Adeguamento NODE_POS, il nodo è PoS

* primitives/block.h

    Classe **CBlockHeader** aggiunto *asset_tag::PoSTag\* tagPoS*, sua gestione e gestione hashPoS

    Definito nuovo flag di versione **SERIALIZE_HEADER_POS** in modo analogo a *SERIALIZE_TRANSACTION_NO_WITNESS*.
    Il flag determina nella serializzazione la gestione della prova PoS.

    Classe **CBlock** adeguamento modifiche CBlockHeader.

    Nella deserializzazione viene verificato se il blocco è PoS e nel caso inizializzato il campo tagPoS

* chain.h txdb.cpp

    Classe **CBlockIndex** aggiunto *CScriptWitness witPoSScript* per mantenere la prova PoS, sua gestione e gestione hashPoS

    Questa modifica impatta sul db *index*, ogni blockIndex PoS cresce della dimension della prova PoS, di un byte se il blocco non è PoS.

    La modifica è necessaria perché in txdb.cpp per ogni record letto viene eseguita la porva PoW sull'header senza dati delle tx del blocco.

* rpc/blockchain.cpp rpc/mining.cpp

    Adeguamento mdifiche CBlock e GetPoSHash

* blockencodings

    Adeguamento mdifiche CBlock per la trasmissione P2P del blocco compatto.

    Nella deserializzazione viene verificato se il blocco è PoS e nel caso inizializzato il campo tagPoS di CBlock

* net_processing.cpp

    La trasmissione e ricezione di HEADERS prevede la versione SERIALIZE_HEADER_POS se il nodo è Pos.
    In tal modo i messaggi HEADERS contengono anche la prova PoS.

* validation.cpp

    Nuova funzione **AcceptBlockPoS** per la gestione di blocchi PoS in CheckBlock e CChainState::AcceptBlock.

    Modifica prova PoW, le chiamate a **CheckProofOfWork** passano la hash della header PoS.

    Riabilitato subsidy per i primi blocchi (da decidere se serve TEST).

    Corretta gestione LOCKTIME_MEDIAN_TIME_PAST al nuovo consensus, la genesi non ha un blocco precedente.

    Modificata verifica Witness commitment per blocchi PoS.

