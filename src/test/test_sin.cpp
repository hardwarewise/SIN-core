// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/test_sin.h>

#include <chainparams.h>
#include <consensus/consensus.h>
#include <consensus/validation.h>
#include <crypto/sha256.h>
#include <validation.h>
#include <init.h>
#include <miner.h>
#include <net_processing.h>
#include <pow.h>
#include <ui_interface.h>
#include <streams.h>
#include <rpc/server.h>
#include <rpc/register.h>
#include <script/sigcache.h>

void CConnmanTest::AddNode(CNode& node)
{
    LOCK(g_connman->cs_vNodes);
    g_connman->vNodes.push_back(&node);
}

void CConnmanTest::ClearNodes()
{
    LOCK(g_connman->cs_vNodes);
    for (CNode* node : g_connman->vNodes) {
        delete node;
    }
    g_connman->vNodes.clear();
}

uint256 insecure_rand_seed = GetRandHash();
FastRandomContext insecure_rand_ctx(insecure_rand_seed);

extern bool fPrintToConsole;
extern void noui_connect();

std::ostream& operator<<(std::ostream& os, const uint256& num)
{
    os << num.ToString();
    return os;
}

BasicTestingSetup::BasicTestingSetup(const std::string& chainName)
    : m_path_root(fs::temp_directory_path() / "test_sin" / strprintf("%lu_%i", (unsigned long)GetTime(), (int)(InsecureRandRange(1 << 30))))
{
    SelectParams(chainName);
    gArgs.ForceSetArg("-printtoconsole", "0");
    if (G_TEST_LOG_FUN) LogInstance().PushBackCallback(G_TEST_LOG_FUN);
    InitLogging();
    LogInstance().StartLogging();
    SHA256AutoDetect();
    RandomInit();
    ECC_Start();
    SetupEnvironment();
    SetupNetworking();
    InitSignatureCache();
    InitScriptExecutionCache();
    fCheckBlockIndex = true;
    noui_connect();
}

BasicTestingSetup::~BasicTestingSetup()
{
    LogInstance().DisconnectTestLogger();
    fs::remove_all(m_path_root);
    ECC_Stop();
}

fs::path BasicTestingSetup::SetDataDir(const std::string& name)
{
    fs::path ret = m_path_root / name;
    fs::create_directories(ret);
    gArgs.ForceSetArg("-datadir", ret.string());
    return ret;
}

TestingSetup::TestingSetup(const std::string& chainName) : BasicTestingSetup(chainName)
{
    SetDataDir("tempdir");
    const CChainParams& chainparams = Params();
    // Ideally we'd move all the RPC tests to the functional testing framework
    // instead of unit tests, but for now we need these here.

    RegisterAllCoreRPCCommands(tableRPC);
    ClearDatadirCache();

    // We have to run a scheduler thread to prevent ActivateBestChain
    // from blocking due to queue overrun.
    threadGroup.create_thread(boost::bind(&CScheduler::serviceQueue, &scheduler));
    GetMainSignals().RegisterBackgroundSignalScheduler(scheduler);

    mempool.setSanityCheck(1.0);
    pblocktree.reset(new CBlockTreeDB(1 << 20, true));
    pcoinsdbview.reset(new CCoinsViewDB(1 << 23, true));
    pcoinsTip.reset(new CCoinsViewCache(pcoinsdbview.get()));
    if (!LoadGenesisBlock(chainparams)) {
        throw std::runtime_error("LoadGenesisBlock failed.");
    }
    {
        CValidationState state;
        if (!ActivateBestChain(state, chainparams)) {
            throw std::runtime_error(strprintf("ActivateBestChain failed. (%s)", FormatStateMessage(state)));
        }
    }
    nScriptCheckThreads = 3;
    for (int i=0; i < nScriptCheckThreads-1; i++)
        threadGroup.create_thread(&ThreadScriptCheck);
    g_connman = std::unique_ptr<CConnman>(new CConnman(0x1337, 0x1337)); // Deterministic randomness for tests.
    connman = g_connman.get();
    peerLogic.reset(new PeerLogicValidation(connman, scheduler, /*enable_bip61=*/true));
}

TestingSetup::~TestingSetup()
{
        threadGroup.interrupt_all();
        threadGroup.join_all();
        GetMainSignals().FlushBackgroundCallbacks();
        GetMainSignals().UnregisterBackgroundSignalScheduler();
        g_connman.reset();
        peerLogic.reset();
        UnloadBlockIndex();
        pcoinsTip.reset();
        pcoinsdbview.reset();
        pblocktree.reset();
}

TestChain100Setup::TestChain100Setup() : TestingSetup(CBaseChainParams::REGTEST)
{
    // CreateAndProcessBlock() does not support building SegWit blocks, so don't activate in these tests.
    // TODO: fix the code to support SegWit blocks.
    UpdateVersionBitsParameters(Consensus::DEPLOYMENT_SEGWIT, 0, Consensus::BIP9Deployment::NO_TIMEOUT);
    // Generate a 100-block chain:
    coinbaseKey.MakeNewKey(true);
    CScript scriptPubKey = CScript() <<  ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
    for (int i = 0; i < COINBASE_MATURITY; i++)
    {
        std::vector<CMutableTransaction> noTxns;
        CBlock b = CreateAndProcessBlock(noTxns, scriptPubKey);
        m_coinbase_txns.push_back(b.vtx[0]);
    }
}

//
// Create a new block with just given transactions, coinbase paying to
// scriptPubKey, and try to add it to the current chain.
//
CBlock
TestChain100Setup::CreateAndProcessBlock(const std::vector<CMutableTransaction>& txns, const CScript& scriptPubKey)
{
    const CChainParams& chainparams = Params();
    std::unique_ptr<CBlockTemplate> pblocktemplate = BlockAssembler(chainparams).CreateNewBlock(scriptPubKey);
    CBlock& block = pblocktemplate->block;

    // Replace mempool-selected txns with just coinbase plus passed-in txns:
    block.vtx.resize(1);
    for (const CMutableTransaction& tx : txns)
        block.vtx.push_back(MakeTransactionRef(tx));
    // IncrementExtraNonce creates a valid coinbase and merkleRoot
    {
        LOCK(cs_main);
        unsigned int extraNonce = 0;
        IncrementExtraNonce(&block, chainActive.Tip(), extraNonce);
    }

    while (!CheckProofOfWork(block.GetHash(), block.nBits, chainparams.GetConsensus())) ++block.nNonce;

    std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(block);
    ProcessNewBlock(chainparams, shared_pblock, true, nullptr);

    CBlock result = block;
    return result;
}

TestChain100Setup::~TestChain100Setup()
{
}


CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(const CMutableTransaction &tx) {
    return FromTx(MakeTransactionRef(tx));
}

CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(const CTransactionRef& tx)
{
    return CTxMemPoolEntry(tx, nFee, nTime, nHeight,
                           spendsCoinbase, sigOpCost, lp);
}

/**
 * @returns a real block (0e56f1c981c90213c34898cf650e11fcfcb3db97c7ca305fd63f489b25206581)
 *      with 7 txs.
 */
CBlock getBlock13b8a()
{
    CBlock block;
    CDataStream stream(ParseHex("0000002079d18f466334f38b9e2078a2448f4f31461933527acfe5cfc3eaa45b50efbc65bbfb532157eb9697153c50db4770c4a3cdcb719f1d2f25653f678ff47f90a185479f2b5f76d90e1c05e05a570702000000010000000000000000000000000000000000000000000000000000000000000000ffffffff18030c3f0704479f2b5f08810009061e0000007969696d70000000000009008fea94000000001976a914e188b4f666175047d58a3b9e9c70ff02bf16b7e688ac89500e76060000001976a914841e6bf56b99a59545da932de2efb23ab93b4f4488ac00a0acb9030000001976a914a23dc63abbc0ed1d87aafaddfcd6e83dc8b7ffef88ac0086de82130000001976a9148ac5375eff68548a70ca24734821ae22a8230fe388ac00d8bbca280000001976a914590d619460a325b92a784a195d2ee79f4be1079488aca0860100000000001976a914566f13021929df6e4cb29b24cc7a98ed13a0b0e488ac20a10700000000001976a9148b939b46226a221e793ca1046392a19dc3241a7f88ac40420f00000000001976a914094e03eed93e7582c5fbc2feca70927b575f29a188ac622e4500000000001976a914ebaf5ec74cb2e2342dfda0229111738ff4dc742d88ac000000000100000002592c76d90b4d23a0f44c0a64be3796df61f33bde3a703c6b45e3d3fcca4601a8000000006a4730440220023022076bcf90025dd9f1bcfe2fb1a1d99192e9e317e5047b9f9069c8914c1e02200be12275c4500646028dd7e6dfe7b582e4987db36c000934569a5e80125f9472012103fa5a0c1480dd8a6f38a9553a304a977c664c93f65d905bca1de1b078c999eaf7feffffff592c76d90b4d23a0f44c0a64be3796df61f33bde3a703c6b45e3d3fcca4601a8010000006a47304402203a1aa34aef0be167fcd358242ddb5f4c7037bca38f2f485fd95422900ccc9f4102202148e25329e5627d493a5b4cd879f4ce5766193d32502b0f56e86ca32c1c22f20121021b47a475ac8686baf146e93357c6b7f7a7e8bc50607f3c799add05880a576412feffffff025063a12b000000001976a914536943d67bd5ef93f3b461c9b0bdc27a55042ffc88ace05a2c29010000001976a91437ca0f42350dd67e085340d39ec36d974a3d139b88ac0b3f070001000000027f373b91846d07d3034f3eb4c031e742463de62a72c1ca378649681b75f76dfc010000006a47304402203e2190d50f8e1ca15b9473baf22489981a1e33663d2210026bdde4c8db3b944b02205c06529638ed0c3fa3e5e0b4ea1dc0661462cf7e198666badb68ee6991354a98012102b2cf190fdd1fb5bae242abe0d76746dad46f9fb77916e8e458fc93deeea1996afeffffff69af7e788ec5a3f78415f75c8f0e72f70e53932c26a14eab1d91e3777f464670010000006a473044022013717ced8faf2c2236fd4507b42cab8d6dde3b6687440d177a0cbea541b4dabf02204e77376426c16c70359d4069185218c169884da5639141066a34c0d7bb87260a012102001128c526bf531e36f193353652c2b08965bd08950aa4eb263eb88ca73c6ff5feffffff028fcb0a1c000000001976a914b747dff2f6ab0981f50547c55ea04b1bcdd0c45288ac0086c9ad020000001976a91437ca0f42350dd67e085340d39ec36d974a3d139b88ac0b3f07000100000002aea51ba0abd9452c76842a455224da6814062298b29384ad9846e57a34e777b6010000006a4730440220217e79ef7b276ea2b34526c91cf8584d442b6127c112271964c8c1f2f46cd95f022045a43049188df01b0321dd03c5dde2e5c7c8b4d625e10ad4807b9271cb81802a012103fa5a0c1480dd8a6f38a9553a304a977c664c93f65d905bca1de1b078c999eaf7feffffff7ae415b2c6a597e8787581bc0c347222c1ebb130480a6744e0a331fe9a1d7698010000006a473044022014110b4c67a6ed4293acad4c808e775548a5264d4a04e3ccdbeb5cb1ba2cbbfa0220470536951ad6b83774aee40212c7d128e13e834ac36e6063c961eb0cac07c123012102b67eccb84259c5230bbee8003c31c71f57521a87d55fe847a6ef33bc4cf0e35cfeffffff02e01feee6020000001976a91437ca0f42350dd67e085340d39ec36d974a3d139b88ac2a628e3d010000001976a914252bcc37937cdcc75e571e7cb8f98c99773ab17988ac0b3f07000100000001cb29dd592d4bdd60ffd6c341feea21574f1858cd3659606f0c41b5424662e1f1010000006a473044022047917de45abfbee17f162d9e410fbffddf1086d4f1ca8ef9a28e045556c9b5e702206fb95032722ffc1316e88d04d774987a9a7dc7fb1b22cfb6709d86c983b563b1012103fa5a0c1480dd8a6f38a9553a304a977c664c93f65d905bca1de1b078c999eaf7feffffff0234d9a408040000001976a914d31b1bd29cddacf3f8b0c51a9e7dbc8d4ed2022c88ac40bdf2f0000000001976a91437ca0f42350dd67e085340d39ec36d974a3d139b88ac0b3f07000100000001e59433560fc59adda753584f352784147af1a74b2f121bef8170cd8db22d3883000000006a47304402202b9ff8b642bc05d8132bb84b4143c9d9555f69868b9cf33ebc0cf4499871949502205e0b4ae8f2584de75894619d9268122065b668cdff85e2f8db8a32f7a2775887012103fa5a0c1480dd8a6f38a9553a304a977c664c93f65d905bca1de1b078c999eaf7feffffff02d4a68920080000001976a914a7a611dd682c1c3b636fa264dff2d75aa9049d8788aca02e807b030000001976a91437ca0f42350dd67e085340d39ec36d974a3d139b88ac0b3f07000100000001fe1733d3d5cc7f3d31f57c49c43d1d7dc6389dc55f497bf83fd4e04ef7610726010000006a473044022057c1bdd250c0d8cb3e687102c24f45f5e56bbca8106b94b1f6f9146f7e8e64a6022035d1ff575f44834cda2d4d02fea00f8b147b62a31dbe82194e032aa88ef1fce30121030b96583b096c59d2dadebe7b83b9b856172e6722bc41277488e9c61a897b9cedfeffffff02293802d5080000001976a914da2e2eb4e1785a6a72922c29dc64f20bc751022188ac3010951f010000001976a91437ca0f42350dd67e085340d39ec36d974a3d139b88ac0b3f0700"), SER_NETWORK, PROTOCOL_VERSION);
    stream >> block;
    return block;
}
