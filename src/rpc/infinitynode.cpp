// Copyright (c) 2018-2019 SIN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <init.h>
#include <netbase.h>
#include <key_io.h>
#include <core_io.h>
#include <validation.h>
#include <infinitynodeman.h>
#include <infinitynodersv.h>
#include <infinitynodemeta.h>
#include <infinitynodepeer.h>
#include <infinitynodelockreward.h>
#ifdef ENABLE_WALLET
#include <wallet/coincontrol.h>
#endif // ENABLE_WALLET
#include <rpc/server.h>
#include <util.h>
#include <utilmoneystr.h>
#include <consensus/validation.h>

#include <secp256k1.h>
#include <secp256k1_schnorr.h>
#include <secp256k1_musig.h>

#include <fstream>
#include <iomanip>
#include <univalue.h>

UniValue infinitynode(const JSONRPCRequest& request)
{
    std::string strCommand;
    std::string strFilter = "";
    std::string strOption = "";
    std::string strError;

    if (request.params.size() >= 1) {
        strCommand = request.params[0].get_str();
    }
    if (request.params.size() == 2) strFilter = request.params[1].get_str();
    if (request.params.size() == 3) {
        strFilter = request.params[1].get_str();
        strOption = request.params[2].get_str();
    }
    if (request.params.size() > 3)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Too many parameters");

    if (request.fHelp  ||
        (strCommand != "build-list" && strCommand != "show-lastscan" && strCommand != "show-infos" && strCommand != "stats"
                                    && strCommand != "show-lastpaid" && strCommand != "build-stm" && strCommand != "show-stm"
                                    && strCommand != "show-candidate" && strCommand != "show-script" && strCommand != "show-proposal"
                                    && strCommand != "scan-vote" && strCommand != "show-proposals" && strCommand != "keypair"
                                    && strCommand != "mypeerinfo" && strCommand != "checkkey" && strCommand != "scan-metadata"
                                    && strCommand != "show-metadata" && strCommand != "memory-lockreward"
                                    && strCommand != "show-lockreward"
        ))
            throw std::runtime_error(
                "infinitynode \"command\"...\n"
                "Set of commands to execute masternode related actions\n"
                "\nArguments:\n"
                "1. \"command\"        (string or set of strings, required) The command to execute\n"
                "\nAvailable commands:\n"
                "  keypair                     - Generation the compressed key pair\n"
                "  checkkey                    - Get info about a privateKey\n"
                "  mypeerinfo                  - Get status of Peer if this node is Infinitynode\n"
                "  build-list                  - Build list of all infinitynode from block height 165000 to last block\n"
                "  build-stm                   - Build statement list from genesis parameter\n"
                "  show-infos                  - Show the list of nodes and last information\n"
                "  show-lastscan               - Last nHeight when list is updated\n"
                "  show-lastpaid               - Last paid of all nodes\n"
                "  show-stm                    - Last statement of each SinType\n"
                "  show-metadata               - Show the list of metadata\n"
                "  show-candidate nHeight      - Show candidata of reward at Height\n"
                "  scan-vote                   - Build list voted\n"
                "  show-lockreward             - LockReward infos for curren Height\n"
                "  scan-metadata               - Build/update list metadata\n"
                "  memory-lockreward           - show the size used by LockReward feature\n"
                );

    UniValue obj(UniValue::VOBJ);

    if (strCommand == "keypair")
    {
        CKey secret;
        secret.MakeNewKey(true);
        CPubKey pubkey = secret.GetPubKey();
        assert(secret.VerifyPubKey(pubkey));

        std::string sBase64 = EncodeBase64(pubkey.begin(), pubkey.size());
        std::vector<unsigned char> tx_data = DecodeBase64(sBase64.c_str());
        CPubKey decodePubKey(tx_data.begin(), tx_data.end());
        CTxDestination dest = GetDestinationForKey(decodePubKey, DEFAULT_ADDRESS_TYPE);

        obj.push_back(Pair("PrivateKey", EncodeSecret(secret)));
        obj.push_back(Pair("PublicKey", sBase64));
        obj.push_back(Pair("DecodePublicKey", decodePubKey.GetID().ToString()));
        obj.push_back(Pair("Address", EncodeDestination(dest)));
        obj.push_back(Pair("isCompressed", pubkey.IsCompressed()));


        return obj;
    }

    if (strCommand == "checkkey")
    {
        std::string strKey = request.params[1].get_str();
        CKey secret = DecodeSecret(strKey);
        if (!secret.IsValid()) throw JSONRPCError(RPC_INTERNAL_ERROR, "Not a valid key");

        CPubKey pubkey = secret.GetPubKey();
        assert(secret.VerifyPubKey(pubkey));

        std::string sBase64 = EncodeBase64(pubkey.begin(), pubkey.size());
        std::vector<unsigned char> tx_data = DecodeBase64(sBase64.c_str());
        CPubKey decodePubKey(tx_data.begin(), tx_data.end());
        CTxDestination dest = GetDestinationForKey(decodePubKey, DEFAULT_ADDRESS_TYPE);

        obj.push_back(Pair("PrivateKey", EncodeSecret(secret)));
        obj.push_back(Pair("PublicKey", sBase64));
        obj.push_back(Pair("DecodePublicKey", decodePubKey.GetID().ToString()));
        obj.push_back(Pair("Address", EncodeDestination(dest)));
        obj.push_back(Pair("isCompressed", pubkey.IsCompressed()));

        return obj;
    }

    if (strCommand == "mypeerinfo")
    {
        if (!fInfinityNode)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "This is not an InfinityNode");

        UniValue infObj(UniValue::VOBJ);
        infinitynodePeer.ManageState(*g_connman);
        infObj.push_back(Pair("MyPeerInfo", infinitynodePeer.GetMyPeerInfo()));
        return infObj;
    }

    if (strCommand == "build-list")
    {
        if (request.params.size() == 1) {
            CBlockIndex* pindex = NULL;
            LOCK(cs_main); // make sure this scopes until we reach the function which needs this most, buildInfinitynodeListRPC()
            pindex = chainActive.Tip();
            return infnodeman.buildInfinitynodeListRPC(pindex->nHeight);
        }

        std::string strMode = request.params[1].get_str();

        if (strMode == "lastscan")
            return infnodeman.getLastScan();
    }

    if (strCommand == "build-stm")
    {
        CBlockIndex* pindex = NULL;
        {
                LOCK(cs_main);
                pindex = chainActive.Tip();
        }
        bool updateStm = false;
        LOCK(cs_main);
        infnodeman.UpdatedBlockTip(pindex);
        if (infnodeman.updateInfinitynodeList(pindex->nHeight)){
            updateStm = infnodeman.deterministicRewardStatement(10) &&
                             infnodeman.deterministicRewardStatement(5) &&
                             infnodeman.deterministicRewardStatement(1);
        }
        obj.push_back(Pair("Height", pindex->nHeight));
        obj.push_back(Pair("Result", updateStm));
        return obj;
    }

    if (strCommand == "show-stm")
    {
        return infnodeman.getLastStatementString();
    }

    if (strCommand == "show-candidate")
    {
        if (request.params.size() != 2)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Correct usage is 'infinitynode show-candidate \"nHeight\"'");
        int nextHeight = 10;
        nextHeight = atoi(strFilter);

        if (nextHeight < Params().GetConsensus().nInfinityNodeGenesisStatement) {
            strError = strprintf("nHeight must be higher than the Genesis Statement height (%s)", Params().GetConsensus().nInfinityNodeGenesisStatement);
            throw JSONRPCError(RPC_INVALID_PARAMETER, strError);
        }

        CInfinitynode infBIG, infMID, infLIL;
        LOCK(infnodeman.cs);
        infnodeman.deterministicRewardAtHeight(nextHeight, 10, infBIG);
        infnodeman.deterministicRewardAtHeight(nextHeight, 5, infMID);
        infnodeman.deterministicRewardAtHeight(nextHeight, 1, infLIL);

        obj.push_back(Pair("Candidate BIG: ", infBIG.getCollateralAddress()));
        obj.push_back(Pair("Candidate MID: ", infMID.getCollateralAddress()));
        obj.push_back(Pair("Candidate LIL: ", infLIL.getCollateralAddress()));

        return obj;
    }

    if (strCommand == "show-lastscan")
    {
            return infnodeman.getLastScan();
    }

    if (strCommand == "show-lastpaid")
    {
        std::map<CScript, int>  mapLastPaid = infnodeman.GetFullLastPaidMap();
        for (auto& pair : mapLastPaid) {
            std::string scriptPublicKey = pair.first.ToString();
            obj.push_back(Pair(scriptPublicKey, pair.second));
        }
        return obj;
    }

    if (strCommand == "show-infos")
    {
        std::map<COutPoint, CInfinitynode> mapInfinitynodes = infnodeman.GetFullInfinitynodeMap();
        std::map<std::string, CMetadata> mapInfMetadata = infnodemeta.GetFullNodeMetadata();
        for (auto& infpair : mapInfinitynodes) {
            std::string strOutpoint = infpair.first.ToStringShort();
            CInfinitynode inf = infpair.second;
            CMetadata meta = mapInfMetadata[inf.getMetaID()];
            std::string nodeAddress = "NodeAddress";

            if (meta.getMetaPublicKey() != "") nodeAddress = meta.getMetaPublicKey();

                std::ostringstream streamInfo;
                streamInfo << std::setw(8) <<
                               inf.getCollateralAddress() << " " <<
                               inf.getHeight() << " " <<
                               inf.getExpireHeight() << " " <<
                               inf.getRoundBurnValue() << " " <<
                               inf.getSINType() << " " <<
                               inf.getBackupAddress() << " " <<
                               inf.getLastRewardHeight() << " " <<
                               inf.getRank() << " " << 
                               infnodeman.getLastStatementSize(inf.getSINType()) << " " <<
                               inf.getMetaID() << " " <<
                               nodeAddress << " " <<
                               meta.getService().ToString()
                               ;
                std::string strInfo = streamInfo.str();
                obj.push_back(Pair(strOutpoint, strInfo));
        }
        return obj;
    }

    if (strCommand == "show-script")
    {
        std::map<COutPoint, CInfinitynode> mapInfinitynodes = infnodeman.GetFullInfinitynodeMap();
        for (auto& infpair : mapInfinitynodes) {
            std::string strOutpoint = infpair.first.ToStringShort();
            CInfinitynode inf = infpair.second;
                std::ostringstream streamInfo;
                        std::vector<std::vector<unsigned char>> vSolutions;
                        txnouttype whichType;
                        const CScript& prevScript = inf.getScriptPublicKey();
                        Solver(prevScript, whichType, vSolutions);
                        std::string backupAddresstmp(vSolutions[1].begin(), vSolutions[1].end());
                streamInfo << std::setw(8) <<
                               inf.getCollateralAddress() << " " <<
                               backupAddresstmp << " " <<
                               inf.getRoundBurnValue();
                std::string strInfo = streamInfo.str();
                obj.push_back(Pair(strOutpoint, strInfo));
        }
        return obj;
    }

    if (strCommand == "show-proposal")
    {
        if (request.params.size() < 2)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Correct usage is 'infinitynode show-proposal \"ProposalId\" \"(Optional)Mode\" '");

        std::string proposalId  = strFilter;
        std::vector<CVote>* vVote = infnodersv.Find(proposalId);
        obj.push_back(Pair("ProposalId", proposalId));
        if(vVote != NULL){
            obj.push_back(Pair("Votes", (int)vVote->size()));
        }else{
            obj.push_back(Pair("Votes", "0"));
        }
        int mode = 0;
        if (strOption == "public"){mode=0;}
        if (strOption == "node"){mode=1;}
        if (strOption == "all"){mode=2;}
        obj.push_back(Pair("Yes", infnodersv.getResult(proposalId, true, mode)));
        obj.push_back(Pair("No", infnodersv.getResult(proposalId, false, mode)));
        for (auto& v : *vVote){
            CTxDestination addressVoter;
            ExtractDestination(v.getVoter(), addressVoter);
            obj.push_back(Pair(EncodeDestination(addressVoter), v.getOpinion()));
        }
        return obj;
    }

    if (strCommand == "show-proposals")
    {
        std::map<std::string, std::vector<CVote>> mapCopy = infnodersv.GetFullProposalVotesMap();
        obj.push_back(Pair("Proposal", (int)mapCopy.size()));
        for (auto& infpair : mapCopy) {
            obj.push_back(Pair(infpair.first, (int)infpair.second.size()));
        }

        return obj;
    }

    if (strCommand == "show-metadata")
    {
        std::map<std::string, CMetadata>  mapCopy = infnodemeta.GetFullNodeMetadata();
        obj.push_back(Pair("Metadata", (int)mapCopy.size()));
        for (auto& infpair : mapCopy) {
            std::ostringstream streamInfo;
                streamInfo << std::setw(8) <<
                               infpair.second.getMetaPublicKey() << " " <<
                               infpair.second.getService().ToString() << " " <<
                               infpair.second.getMetadataHeight();
                std::string strInfo = streamInfo.str();

            UniValue metaHisto(UniValue::VARR);
            for(auto& v : infpair.second.getHistory()){
                 std::ostringstream vHistoMeta;
                 vHistoMeta << std::setw(4) <<
                     v.nHeightHisto  << " " <<
                     v.pubkeyHisto << " " <<
                     v.serviceHisto.ToString();
                 std::string strHistoMeta = vHistoMeta.str();
                 metaHisto.push_back(strHistoMeta);
            }
            obj.push_back(Pair(infpair.first, strInfo));
            std::string metaHistStr = strprintf("History %s", infpair.first);
            obj.push_back(Pair(metaHistStr, metaHisto));
        }
        return obj;
    }

    if (strCommand == "scan-vote")
    {
        CBlockIndex* pindex = NULL;
        {
                LOCK(cs_main);
                pindex = chainActive.Tip();
        }

        bool result = infnodersv.rsvScan(pindex->nHeight);
        obj.push_back(Pair("Result", result));
        obj.push_back(Pair("Details", infnodersv.ToString()));
        return obj;
    }

    if (strCommand == "scan-metadata")
    {
        CBlockIndex* pindex = NULL;
        {
                LOCK(cs_main);
                pindex = chainActive.Tip();
        }

        bool result = infnodemeta.metaScan(pindex->nHeight);
        obj.push_back(Pair("Result", result));
        return obj;
    }

    if (strCommand == "memory-lockreward")
    {
        obj.push_back(Pair("LockReward", inflockreward.GetMemorySize()));
        return obj;
    }

    if (strCommand == "show-lockreward")
    {
        CBlockIndex* pindex = NULL;

        LOCK(cs_main);
        pindex = chainActive.Tip();

        std::vector<CLockRewardExtractInfo> vecLockRewardRet;
        if (request.params.size() < 2) vecLockRewardRet = infnodelrinfo.getFullLRInfo();
        if (request.params.size() == 2){
            int nBlockNumber  = atoi(strFilter);
            infnodelrinfo.getLRInfo(nBlockNumber, vecLockRewardRet);
        }

        obj.push_back(Pair("Result", (int)vecLockRewardRet.size()));
        obj.push_back(Pair("Current height", pindex->nHeight));
        int i=0;
        for (auto& v : vecLockRewardRet) {
                std::ostringstream streamInfo;
                CTxDestination address;
                bool fValidAddress = ExtractDestination(v.scriptPubKey, address);

                std::string owner = "Unknow";
                if(fValidAddress) owner = EncodeDestination(address);

                streamInfo << std::setw(1) <<
                               v.nSINtype << " " <<
                               owner  << " " <<
                               v.sLRInfo;
                std::string strInfo = streamInfo.str();
                obj.push_back(Pair(strprintf("%d-%d",v.nBlockHeight, i), strInfo));
            i++;
        }
        return obj;
    }

    return NullUniValue;
}

/**
 * @xtdevcoin
 * this function help user burn correctly their funds to run infinity node
 */
static UniValue infinitynodeburnfund(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (request.fHelp || request.params.size() != 3)
       throw std::runtime_error(
            "infinitynodeburnfund NodeOwnerAddress amount SINBackupAddress"
            "\nSend an amount to BurnAddress.\n"
            "\nArguments:\n"
            "1. \"NodeOwnerAddress\" (string, required) Address of Collateral.\n"
            "2. \"amount\"             (numeric or string, required) The amount in " + CURRENCY_UNIT + " to send. eg 0.1\n"
            "3. \"NodeOwnerBackupAddress\"  (string, required) The SIN address to send to when you make a notification(new feature soon).\n"
            "\nResult:\n"
            "\"BURNtxid\"                  (string) The Burn transaction id. Need to run infinity node\n"
            "\"CollateralAddress\"         (string) Address of Collateral. Please send 10000 SIN to this address.\n"
            "\nExamples:\n"
            + HelpExampleCli("infinitynodeburnfund", "NodeOwnerAddress 1000000 SINBackupAddress")
        );

    EnsureWalletIsUnlocked(pwallet);
    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    // Grab locks here as BlockUntilSyncedToCurrentChain() handles them on its own, but we need them for most other funcs
    LOCK2(cs_main, pwallet->cs_wallet);

    std::string strError;
    std::vector<COutput> vPossibleCoins;
    pwallet->AvailableCoins(vPossibleCoins, true, NULL, false, ALL_COINS);

    UniValue results(UniValue::VARR);
    // Amount

    CTxDestination NodeOwnerAddress = DecodeDestination(request.params[0].get_str());
    if (!IsValidDestination(NodeOwnerAddress))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid SIN address for NodeOwnerAddress");

    CAmount nAmount = AmountFromValue(request.params[1]);
    if (nAmount != Params().GetConsensus().nMasternodeBurnSINNODE_1 * COIN &&
        nAmount != Params().GetConsensus().nMasternodeBurnSINNODE_5 * COIN &&
        nAmount != Params().GetConsensus().nMasternodeBurnSINNODE_10 * COIN)
    {
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount to burn and run an InfinityNode");
    }

    CTxDestination BKaddress = DecodeDestination(request.params[2].get_str());
    if (!IsValidDestination(BKaddress))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid SIN address for Backup");

    std::map<COutPoint, CInfinitynode> mapInfinitynodes = infnodeman.GetFullInfinitynodeMap();
    int totalNode = 0, totalBIG = 0, totalMID = 0, totalLIL = 0, totalUnknown = 0;
    for (auto& infpair : mapInfinitynodes) {
        ++totalNode;
        CInfinitynode inf = infpair.second;
        int sintype = inf.getSINType();
        if (sintype == 10) ++totalBIG;
        else if (sintype == 5) ++totalMID;
        else if (sintype == 1) ++totalLIL;
        else ++totalUnknown;
    }

    // BurnAddress
    CTxDestination dest = DecodeDestination(Params().GetConsensus().cBurnAddress);
    CScript scriptPubKeyBurnAddress = GetScriptForDestination(dest);
    std::vector<std::vector<unsigned char> > vSolutions;
    txnouttype whichType;
    if (!Solver(scriptPubKeyBurnAddress, whichType, vSolutions))
        return false;
    CKeyID keyid = CKeyID(uint160(vSolutions[0]));

    // Wallet comments
    std::set<CTxDestination> destinations;
    LOCK(pwallet->cs_wallet);
    for (COutput& out : vPossibleCoins) {
        CTxDestination address;
        const CScript& scriptPubKey = out.tx->tx->vout[out.i].scriptPubKey;
        bool fValidAddress = ExtractDestination(scriptPubKey, address);

        if (!fValidAddress || address != NodeOwnerAddress)
            continue;

        UniValue entry(UniValue::VOBJ);
        entry.pushKV("txid", out.tx->GetHash().GetHex());
        entry.pushKV("vout", out.i);

        if (fValidAddress) {
            entry.pushKV("address", EncodeDestination(address));

            auto i = pwallet->mapAddressBook.find(address);
            if (i != pwallet->mapAddressBook.end()) {
                entry.pushKV("label", i->second.name);
                if (IsDeprecatedRPCEnabled("accounts")) {
                    entry.pushKV("account", i->second.name);
                }
            }

            if (scriptPubKey.IsPayToScriptHash()) {
                const CScriptID& hash = boost::get<CScriptID>(address);
                CScript redeemScript;
                if (pwallet->GetCScript(hash, redeemScript)) {
                    entry.pushKV("redeemScript", HexStr(redeemScript.begin(), redeemScript.end()));
                }
            }
        }

        entry.pushKV("scriptPubKey", HexStr(scriptPubKey.begin(), scriptPubKey.end()));
        entry.pushKV("amount", ValueFromAmount(out.tx->tx->vout[out.i].nValue));
        entry.pushKV("spendable", out.fSpendable);
        entry.pushKV("solvable", out.fSolvable);
        entry.pushKV("safe", out.fSafe);
        if (out.tx->tx->vout[out.i].nValue >= nAmount && out.nDepth >= 2) {
            /*check address is unique*/
            for (auto& infpair : mapInfinitynodes) {
                CInfinitynode inf = infpair.second;
                if(inf.getCollateralAddress() == EncodeDestination(address)){
                    strError = strprintf("Error: Address %s exist in list. Please use another address to make sure it is unique.", EncodeDestination(address));
                    throw JSONRPCError(RPC_TYPE_ERROR, strError);
                }
            }
            // Wallet comments
            mapValue_t mapValue;
            bool fSubtractFeeFromAmount = true;
            CCoinControl coin_control;
            coin_control.Select(COutPoint(out.tx->GetHash(), out.i));
            coin_control.destChange = NodeOwnerAddress;//fund go back to NodeOwnerAddress

            CScript script;
            script = GetScriptForBurn(keyid, request.params[2].get_str());

            CReserveKey reservekey(pwallet);
            CAmount nFeeRequired;
            CAmount curBalance = pwallet->GetBalance();
            
            std::vector<CRecipient> vecSend;
            int nChangePosRet = -1;
            CRecipient recipient = {script, nAmount, fSubtractFeeFromAmount};
            vecSend.push_back(recipient);
            CTransactionRef tx;
            if (!pwallet->CreateTransaction(vecSend, tx, reservekey, nFeeRequired, nChangePosRet, strError, coin_control, true, ALL_COINS)) {
                if (!fSubtractFeeFromAmount && nAmount + nFeeRequired > curBalance)
                    strError = strprintf("Error: This transaction requires a transaction fee of at least %s", FormatMoney(nFeeRequired));
                throw JSONRPCError(RPC_WALLET_ERROR, strError);
            }
            CValidationState state;
            if (!pwallet->CommitTransaction(tx, std::move(mapValue), {} /* orderForm */, {}/*fromAccount*/, reservekey, g_connman.get(),
                            state, NetMsgType::TX)) {
                strError = strprintf("Error: The transaction was rejected! Reason given: %s", FormatStateMessage(state));
                throw JSONRPCError(RPC_WALLET_ERROR, strError);
            }
            entry.pushKV("BURNADDRESS", EncodeDestination(dest));
            entry.pushKV("BURNPUBLICKEY", HexStr(keyid.begin(), keyid.end()));
            entry.pushKV("BURNSCRIPT", HexStr(scriptPubKeyBurnAddress.begin(), scriptPubKeyBurnAddress.end()));
            entry.pushKV("BURNTX", tx->GetHash().GetHex());
            entry.pushKV("OWNER_ADDRESS",EncodeDestination(address));
            entry.pushKV("BACKUP_ADDRESS",EncodeDestination(BKaddress));
            //coins is good to burn
            results.push_back(entry);
            break; //immediat
        }
    }
    return results;
}

 
static UniValue infinitynodeupdatemeta(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (request.fHelp || request.params.size() != 4)
       throw std::runtime_error(
            "infinitynodeupdatemeta INFAddress UpdateInfo"
            "\nSend update info.\n"
            "\nArguments:\n"
            "1. \"OwnerAddress\"      (string, required) Address of node OWNER which funds are burnt.\n"
            "2. \"PublicKey\"         (string, required) PublicKey of node which will be used for LockReward, FlashSend...\n"
            "3. \"IP\"                (string, required) IP of node.\n"
            "4. \"NodeBurnFundTx\"    (string, required) 16 characters.\n"
            "\nResult:\n"
            "\"UpdateInfo upadted message\"   (string) Metadata information\n"
            "\nExamples:\n"
            + HelpExampleCli("infinitynodeupdatemeta", "OwnerAddress NodeAddress IP")
        );
    UniValue results(UniValue::VOBJ);

    std::string strOwnerAddress = request.params[0].get_str();
    CTxDestination INFAddress = DecodeDestination(strOwnerAddress);
    if (!IsValidDestination(INFAddress)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid SIN address: OwnerAddress");
    }

    //limit data carrier, so we accept only 66 char
    std::string nodePublickeyHexStr = "";
    if(request.params[1].get_str().length() == 44){
        nodePublickeyHexStr = request.params[1].get_str();
    } else {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid node publickey");
    }

    std::string strService = request.params[2].get_str();
    CService service;
    if(Params().NetworkIDString() != CBaseChainParams::REGTEST) {
        if (!Lookup(strService.c_str(), service, 0, false)){
               throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "IP address is not valid");
        }
    }

    std::string burnfundTxID = "";
    if(request.params[3].get_str().length() == 16){
        burnfundTxID = request.params[3].get_str();
    } else {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "node BurnFundTx ID is invalid. Please enter first 16 characters of BurnFundTx");
    }

    std::string metaID = strprintf("%s-%s", strOwnerAddress, burnfundTxID);
    CMetadata myMeta = infnodemeta.Find(metaID);
    int nCurrentHeight = (int)chainActive.Height();
    if(myMeta.getMetadataHeight() > 0 && nCurrentHeight < myMeta.getMetadataHeight() + Params().MaxReorganizationDepth() * 2){
        int nWait = myMeta.getMetadataHeight() + Params().MaxReorganizationDepth() * 2 - nCurrentHeight;
        std::string strError = strprintf("Error: Please wait %d blocks and try to update again.", nWait);
        throw JSONRPCError(RPC_TYPE_ERROR, strError);
    }

    EnsureWalletIsUnlocked(pwallet);
    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    // Grab locks here as BlockUntilSyncedToCurrentChain() handles them on its own, but we need them for most other funcs
    LOCK2(cs_main, pwallet->cs_wallet);

    std::string strError;
    std::vector<COutput> vPossibleCoins;
    pwallet->AvailableCoins(vPossibleCoins, true, NULL, false, ALL_COINS);

    // cMetadataAddress
    CTxDestination dest = DecodeDestination(Params().GetConsensus().cMetadataAddress);
    CScript scriptPubKeyMetaAddress = GetScriptForDestination(dest);
    std::vector<std::vector<unsigned char> > vSolutions;
    txnouttype whichType;
    if (!Solver(scriptPubKeyMetaAddress, whichType, vSolutions))
            return false;
    CKeyID keyid = CKeyID(uint160(vSolutions[0]));

    std::ostringstream streamInfo;

    for (COutput& out : vPossibleCoins) {
        CTxDestination address;
        const CScript& scriptPubKey = out.tx->tx->vout[out.i].scriptPubKey;
        bool fValidAddress = ExtractDestination(scriptPubKey, address);

        if (!fValidAddress) continue;
        if (EncodeDestination(INFAddress) != EncodeDestination(address)) continue;
        //use coin with limit value
        if (out.tx->tx->vout[out.i].nValue / COIN >= Params().GetConsensus().nInfinityNodeUpdateMeta
            && out.tx->tx->vout[out.i].nValue / COIN < Params().GetConsensus().nInfinityNodeUpdateMeta*100
            && out.nDepth >= 2) {
            CAmount nAmount = Params().GetConsensus().nInfinityNodeUpdateMeta*COIN;
            mapValue_t mapValue;
            bool fSubtractFeeFromAmount = true;
            CCoinControl coin_control;
            coin_control.Select(COutPoint(out.tx->GetHash(), out.i));
            coin_control.destChange = INFAddress;

            streamInfo << nodePublickeyHexStr << ";" << strService << ";" << burnfundTxID;
            std::string strInfo = streamInfo.str();
            CScript script;
            script = GetScriptForBurn(keyid, streamInfo.str());

            CReserveKey reservekey(pwallet);
            CAmount nFeeRequired;
            CAmount curBalance = pwallet->GetBalance();
            
            std::vector<CRecipient> vecSend;
            int nChangePosRet = -1;
            CRecipient recipient = {script, nAmount, fSubtractFeeFromAmount};
            vecSend.push_back(recipient);

            results.push_back(Pair("Metadata",streamInfo.str()));


            CTransactionRef tx;
            if (!pwallet->CreateTransaction(vecSend, tx, reservekey, nFeeRequired, nChangePosRet, strError, coin_control, true, ALL_COINS)) {
                if (!fSubtractFeeFromAmount && nAmount + nFeeRequired > curBalance)
                    strError = strprintf("Error: This transaction requires a transaction fee of at least %s", FormatMoney(nFeeRequired));
                throw JSONRPCError(RPC_WALLET_ERROR, strError);
            }
            CValidationState state;

            if (!pwallet->CommitTransaction(tx, std::move(mapValue), {}, {}, reservekey, g_connman.get(),
                            state, NetMsgType::TX)) {
                strError = strprintf("Error: The transaction was rejected! Reason given: %s", FormatStateMessage(state));
                throw JSONRPCError(RPC_WALLET_ERROR, strError);
            }

            break; //immediat
        }
    }

    return results;
}


static UniValue infinitynodevote(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (request.fHelp || request.params.size() != 3)
       throw std::runtime_error(
            "infinitynodevote AddressVote ProposalId [yes/no]"
            "\nSend update info.\n"
            "\nArguments:\n"
            "1. \"AddressVote\"  (string, required) Address of vote.\n"
            "2. \"ProposalId\"   (string, required) Vote for proposalId\n"
            "3. \"[yes/no]\"            (string, required) opinion.\n"
            "\nResult:\n"
            "\"Vote information\"   (string) result of vote\n"
            "\nExamples:\n"
            + HelpExampleCli("infinitynodevote", "AddressVote ProposalId [yes/no]")
        );
    UniValue results(UniValue::VOBJ);
    std::string strError = "";

    std::string strOwnerAddress = request.params[0].get_str();
    CTxDestination INFAddress = DecodeDestination(strOwnerAddress);
    if (!IsValidDestination(INFAddress)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid SIN address: OwnerAddress");
    }

    std::string ProposalId = request.params[1].get_str();
    bool has_only_digits = (ProposalId.find_first_not_of( "0123456789" ) == string::npos);
    //if (!has_only_digits || ProposalId.size() != 8){
    if (!has_only_digits || ProposalId != "10000000"){//it will be update at hardfork
        strError = strprintf("ProposalID %s must be in format xxxxxxxx (8 digits) number.", ProposalId);
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strError);
    }

    std::string opinion = request.params[2].get_str();
    transform(opinion.begin(), opinion.end(), opinion.begin(), ::toupper);
    std::string vote = "0";
    if (opinion != "YES" && opinion != "NO"){
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Please give your opinion yes or no.");
    }
    if (opinion == "YES") {vote = "1";}

    EnsureWalletIsUnlocked(pwallet);
    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();
    // Grab locks here as BlockUntilSyncedToCurrentChain() handles them on its own, but we need them for most other funcs
    LOCK2(cs_main, pwallet->cs_wallet);

    std::vector<COutput> vPossibleCoins;
    pwallet->AvailableCoins(vPossibleCoins, true, NULL, false, ALL_COINS);

    // cBurnAddress
    CTxDestination dest = DecodeDestination(Params().GetConsensus().cGovernanceAddress);
    CScript scriptPubKeyBurnAddress = GetScriptForDestination(dest);
    std::vector<std::vector<unsigned char> > vSolutions;
    txnouttype whichType;
    if (!Solver(scriptPubKeyBurnAddress, whichType, vSolutions))
        return false;
    CKeyID keyid = CKeyID(uint160(vSolutions[0]));

    std::ostringstream streamInfo;

    for (COutput& out : vPossibleCoins) {
        CTxDestination address;
        const CScript& scriptPubKey = out.tx->tx->vout[out.i].scriptPubKey;
        bool fValidAddress = ExtractDestination(scriptPubKey, address);

        if (!fValidAddress) continue;
        if (EncodeDestination(INFAddress) != EncodeDestination(address)) continue;
        //use coin with limit value 10k SIN
        if (out.tx->tx->vout[out.i].nValue / COIN >= Params().GetConsensus().nInfinityNodeVoteValue
            && out.tx->tx->vout[out.i].nValue / COIN < Params().GetConsensus().nInfinityNodeVoteValue*1000
            && out.nDepth >= 2) {
            CAmount nAmount = Params().GetConsensus().nInfinityNodeVoteValue*COIN;
            mapValue_t mapValue;
            bool fSubtractFeeFromAmount = false;
            CCoinControl coin_control;
            coin_control.Select(COutPoint(out.tx->GetHash(), out.i));
            coin_control.destChange = INFAddress;

            streamInfo << ProposalId << vote;
            std::string strInfo = streamInfo.str();
            CScript script;
            script = GetScriptForBurn(keyid, streamInfo.str());

            CReserveKey reservekey(pwallet);
            CAmount nFeeRequired;
            CAmount curBalance = pwallet->GetBalance();

            std::vector<CRecipient> vecSend;
            int nChangePosRet = -1;
            CRecipient recipient = {script, nAmount, fSubtractFeeFromAmount};
            vecSend.push_back(recipient);

            results.push_back(Pair("Vote",streamInfo.str()));

            CTransactionRef tx;
            if (!pwallet->CreateTransaction(vecSend, tx, reservekey, nFeeRequired, nChangePosRet, strError, coin_control, true, ALL_COINS)) {
                if (!fSubtractFeeFromAmount && nAmount + nFeeRequired > curBalance)
                    strError = strprintf("Error: This transaction requires a transaction fee of at least %s", FormatMoney(nFeeRequired));
                throw JSONRPCError(RPC_WALLET_ERROR, strError);
            }
            CValidationState state;

            if (!pwallet->CommitTransaction(tx, std::move(mapValue), {}, {}, reservekey, g_connman.get(),
                            state, NetMsgType::TX)) {
                strError = strprintf("Error: The transaction was rejected! Reason given: %s", FormatStateMessage(state));
                throw JSONRPCError(RPC_WALLET_ERROR, strError);
            }

            break; //immediat
        }
    }

    return results;
}


static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
  //  --------------------- ------------------------  -----------------------  ----------
    { "SIN",                "infinitynodeburnfund",   &infinitynodeburnfund,   {"amount"} },
    { "SIN",                "infinitynodeupdatemeta", &infinitynodeupdatemeta, {"owner_address","node_address","IP"} },
    { "SIN",                "infinitynode",           &infinitynode,           {"command"}  },
    { "SIN",                "infinitynodevote",       &infinitynodevote,       {"owner_address","proposalid","opinion"} }
};

void RegisterInfinitynodeRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}

