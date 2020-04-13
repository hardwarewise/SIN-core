// Copyright (c) 2018-2019 SIN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <infinitynodelockreward.h>
#include <infinitynodeman.h>
#include <infinitynodepeer.h>
#include <infinitynodemeta.h>
#include <messagesigner.h>
#include <net_processing.h>
#include <netmessagemaker.h>
#include <utilstrencodings.h>

#include <boost/lexical_cast.hpp>

/** Object for who's going to get paid on which blocks */
CInfinityNodeLockReward inflockreward;

/*************************************************************/
/***** CLockRewardRequest ************************************/
/*************************************************************/
CLockRewardRequest::CLockRewardRequest()
{}

CLockRewardRequest::CLockRewardRequest(int Height, COutPoint outpoint, int sintype, int loop){
    nRewardHeight = Height;
    burnTxIn = CTxIn(outpoint);
    nSINtype = sintype;
    nLoop = loop;
}

bool CLockRewardRequest::Sign(const CKey& keyInfinitynode, const CPubKey& pubKeyInfinitynode)
{
    std::string strError;
    std::string strSignMessage;

    std::string strMessage = boost::lexical_cast<std::string>(nRewardHeight) + burnTxIn.ToString()
                             + boost::lexical_cast<std::string>(nSINtype)
                             + boost::lexical_cast<std::string>(nLoop);

    if(!CMessageSigner::SignMessage(strMessage, vchSig, keyInfinitynode)) {
        LogPrintf("CLockRewardRequest::Sign -- SignMessage() failed\n");
        return false;
    }

    if(!CMessageSigner::VerifyMessage(pubKeyInfinitynode, vchSig, strMessage, strError)) {
        LogPrintf("CLockRewardRequest::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CLockRewardRequest::CheckSignature(CPubKey& pubKeyInfinitynode, int &nDos)
{
    std::string strMessage = boost::lexical_cast<std::string>(nRewardHeight) + burnTxIn.ToString()
                             + boost::lexical_cast<std::string>(nSINtype)
                             + boost::lexical_cast<std::string>(nLoop);
    std::string strError = "";

    if(!CMessageSigner::VerifyMessage(pubKeyInfinitynode, vchSig, strMessage, strError)) {
        LogPrintf("CLockRewardRequest::CheckSignature -- Got bad Infinitynode LockReward signature, ID=%s, error: %s\n", 
                    burnTxIn.prevout.ToStringShort(), strError);
        /*TODO: set ban value befor release*/
        nDos = 0;
        return false;
    }
    return true;
}

bool CLockRewardRequest::IsValid(CNode* pnode, int nValidationHeight, std::string& strError, CConnman& connman)
{
    CInfinitynode inf;
    if(!infnodeman.deterministicRewardAtHeight(nRewardHeight, nSINtype, inf)){
        strError = strprintf("Cannot find candidate for Height of LockRequest: %d and SINtype: %d\n", nRewardHeight, nSINtype);
        return false;
    }

    if(inf.vinBurnFund != burnTxIn){
        strError = strprintf("Node %s is not a Candidate for height: %d, SIN type: %d\n", burnTxIn.prevout.ToStringShort(), nRewardHeight, nSINtype);
        return false;
    }

    int nDos = 20;
    CMetadata meta = infnodemeta.Find(inf.getMetaID());
    std::string metaPublicKey = meta.getMetaPublicKey();
    std::vector<unsigned char> tx_data = DecodeBase64(metaPublicKey.c_str());
    CPubKey pubKey(tx_data.begin(), tx_data.end());

    if(!CheckSignature(pubKey, nDos)){
        LOCK(cs_main);
        Misbehaving(pnode->GetId(), nDos);
        strError = strprintf("ERROR: invalid signature of Infinitynode: %s, MetadataID: %s,  PublicKey: %s\n", 
            burnTxIn.prevout.ToStringShort(), inf.getMetaID(), metaPublicKey);

        return false;
    }

    return true;
}

void CLockRewardRequest::Relay(CConnman& connman)
{

    CInv inv(MSG_LOCKREWARD_INIT, GetHash());
    connman.RelayInv(inv);
}

/*************************************************************/
/***** CLockRewardCommitment *********************************/
/*************************************************************/
CLockRewardCommitment::CLockRewardCommitment()
{}

CLockRewardCommitment::CLockRewardCommitment(uint256 nRequest, COutPoint myPeerBurnTxIn, CKey key){
    vin=(CTxIn(myPeerBurnTxIn));
    random = key;
    pubkeyR = key.GetPubKey();
    nHashRequest = nRequest;
    nonce = GetRandInt(999999);
}

bool CLockRewardCommitment::Sign(const CKey& keyInfinitynode, const CPubKey& pubKeyInfinitynode)
{
    std::string strError;
    std::string strSignMessage;

    std::string strMessage = boost::lexical_cast<std::string>(nonce) + nHashRequest.ToString();

    if(!CMessageSigner::SignMessage(strMessage, vchSig, keyInfinitynode)) {
        LogPrintf("CLockRewardCommitment::Sign -- SignMessage() failed\n");
        return false;
    }

    if(!CMessageSigner::VerifyMessage(pubKeyInfinitynode, vchSig, strMessage, strError)) {
        LogPrintf("CLockRewardCommitment::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CLockRewardCommitment::CheckSignature(CPubKey& pubKeyInfinitynode, int &nDos)
{
    std::string strMessage = boost::lexical_cast<std::string>(nonce) + nHashRequest.ToString();
    std::string strError = "";

    if(!CMessageSigner::VerifyMessage(pubKeyInfinitynode, vchSig, strMessage, strError)) {
        LogPrintf("CLockRewardCommitment::CheckSignature -- Got bad Infinitynode LockReward signature, error: %s\n", strError);
        /*TODO: set ban value befor release*/
        nDos = 0;
        return false;
    }
    return true;
}

void CLockRewardCommitment::Relay(CConnman& connman)
{

    CInv inv(MSG_INFCOMMITMENT, GetHash());
    connman.RelayInv(inv);
}

/*************************************************************/
/***** CInfinityNodeLockReward *******************************/
/*************************************************************/
void CInfinityNodeLockReward::Clear()
{
    LOCK(cs);
    mapLockRewardRequest.clear();
    mapLockRewardCommitment.clear();
    mapSigners.clear();
}

bool CInfinityNodeLockReward::AlreadyHave(const uint256& hash)
{
    LOCK(cs);
    return mapLockRewardRequest.count(hash) ||
            mapLockRewardCommitment.count(hash);
}

bool CInfinityNodeLockReward::AddLockRewardRequest(const CLockRewardRequest& lockRewardRequest)
{
    AssertLockHeld(cs);
    //LogPrintf("CInfinityNodeLockReward::AddLockRewardRequest -- lockRewardRequest from %s, SIN type: %d, loop: %d\n",
    //           lockRewardRequest.burnTxIn.prevout.ToStringShort(), lockRewardRequest.nSINtype, lockRewardRequest.nLoop);

    //if we hash this request => don't add it
    if(mapLockRewardRequest.count(lockRewardRequest.GetHash())) return false;
    if(lockRewardRequest.nLoop == 0){
        mapLockRewardRequest.insert(make_pair(lockRewardRequest.GetHash(), lockRewardRequest));
    } else if (lockRewardRequest.nLoop > 0){
        //new request from candidate, so remove old requrest
        RemoveLockRewardRequest(lockRewardRequest);
        mapLockRewardRequest.insert(make_pair(lockRewardRequest.GetHash(), lockRewardRequest));
    }
    return true;
}

bool CInfinityNodeLockReward::GetLockRewardRequest(const uint256& reqHash, CLockRewardRequest& lockRewardRequestRet)
{
    LOCK(cs);
    std::map<uint256, CLockRewardRequest>::iterator it = mapLockRewardRequest.find(reqHash);
    if(it == mapLockRewardRequest.end()) return false;
    lockRewardRequestRet = it->second;
    return true;
}

void CInfinityNodeLockReward::RemoveLockRewardRequest(const CLockRewardRequest& lockRewardRequest)
{
    AssertLockHeld(cs);

    std::map<uint256, CLockRewardRequest>::iterator itRequest = mapLockRewardRequest.begin();
    while(itRequest != mapLockRewardRequest.end()) {
        if(itRequest->second.nRewardHeight == lockRewardRequest.nRewardHeight
            && itRequest->second.burnTxIn == lockRewardRequest.burnTxIn
            && itRequest->second.nSINtype == lockRewardRequest.nSINtype
            && itRequest->second.nLoop < lockRewardRequest.nLoop)
        {
            mapLockRewardRequest.erase(itRequest++);
        }else{
            ++itRequest;
        }
    }
}

bool CInfinityNodeLockReward::AddCommitment(const CLockRewardCommitment& commitment)
{
    AssertLockHeld(cs);

    if(mapLockRewardCommitment.count(commitment.GetHash())) return false;
        mapLockRewardCommitment.insert(make_pair(commitment.GetHash(), commitment));
    return true;
}

bool CInfinityNodeLockReward::GetLockRewardCommitment(const uint256& reqHash, CLockRewardCommitment& commitmentRet)
{
    LOCK(cs);
    std::map<uint256, CLockRewardCommitment>::iterator it = mapLockRewardCommitment.find(reqHash);
    if(it == mapLockRewardCommitment.end()) return false;
    commitmentRet = it->second;
    return true;
}

/*
 * STEP 1: get a new LockRewardRequest, check it and send verify message
 *
 * STEP 1.1 check LockRewardRequest is valid or NOT
 * - the nHeight of request cannot be lower than current height
 * - the nHeight of request cannot so far in future ( currentHeight + Params().GetConsensus().nInfinityNodeCallLockRewardDeepth + 2)
 * - the LockRewardRequest must be valid
 *   + from (nHeight, SINtype) => find candidate
 *   + CTxIn of request and CTxIn of candidate must be identical
 *   + Find publicKey of Candidate in metadata and check the signature in request
 */
bool CInfinityNodeLockReward::CheckLockRewardRequest(CNode* pfrom, CLockRewardRequest& lockRewardRequestRet, CConnman& connman, int nBlockHeight)
{
    AssertLockHeld(cs);
    if(lockRewardRequestRet.nRewardHeight > nBlockHeight + Params().GetConsensus().nInfinityNodeCallLockRewardDeepth + 2
        || lockRewardRequestRet.nRewardHeight < nBlockHeight){
        LogPrintf("CInfinityNodeLockReward::CheckLockRewardRequest -- LockRewardRequest for invalid height: %d, current height: %d\n",
            lockRewardRequestRet.nRewardHeight, nBlockHeight);
        return false;
    }

    std::string strError = "";
    if(!lockRewardRequestRet.IsValid(pfrom, nBlockHeight, strError, connman)){
        LogPrintf("CInfinityNodeLockReward::CheckLockRewardRequest -- LockRewardRequest is invalid. ERROR: %s\n",strError);
        return false;
    }

    return true;
}

/*
 * STEP 1: new LockRewardRequest is OK, check mypeer is a TopNode for LockRewardRequest
 *
 * STEP 1.2 send VerifyRequest
 */

bool CInfinityNodeLockReward::VerifyMyPeerAndSendVerifyRequest(CNode* pfrom, CLockRewardRequest& lockRewardRequestRet, CConnman& connman)
{
    // only Infinitynode will answer the verify LockRewardCandidate
    if(fLiteMode || !fInfinityNode) {return false;}

    AssertLockHeld(cs);

    //step 1.2.2.1: get InfCandidate from request. In fact, this step can not false because it is checked in CheckLockRewardRequest
    CInfinitynode infCandidate;
    if(!infnodeman.Get(lockRewardRequestRet.burnTxIn.prevout, infCandidate)){
        LogPrintf("CInfinityNodeLockReward::VerifyMyPeerAndSendVerifyRequest -- Cannot identify candidate in list\n");
        return false;
    }

    CMetadata metaCandidate = infnodemeta.Find(infCandidate.getMetaID());
    if(metaCandidate.getMetadataHeight() == 0){
        LogPrintf("CInfinityNodeLockReward::VerifyMyPeerAndSendVerifyRequest -- Cannot get metadata of candidate %s\n", infCandidate.getBurntxOutPoint().ToStringShort());
        return false;
    }

    //LockRewardRequest of expired candidate is relayed => ban it
    if(infCandidate.getExpireHeight() < lockRewardRequestRet.nRewardHeight){
        LogPrintf("CInfinityNodeLockReward::VerifyMyPeerAndSendVerifyRequest -- Candidate is expired\n", infCandidate.getBurntxOutPoint().ToStringShort());
        LOCK(cs_main);
        Misbehaving(pfrom->GetId(), 20);
        return false;
    }

    //step 1.2.1: chech if mypeer is good candidate to make Musig
    CInfinitynode infRet;
    if(!infnodeman.Get(infinitynodePeer.burntx, infRet)){
        LogPrintf("CInfinityNodeLockReward::VerifyMyPeerAndSendVerifyRequest -- Cannot identify mypeer in list: %s\n", infinitynodePeer.burntx.ToStringShort());
        return false;
    }

    int nScore;
    int nSINtypeCanLockReward = Params().GetConsensus().nInfinityNodeLockRewardSINType; //mypeer must be this SINtype, if not, score is NULL

    if(!infnodeman.getNodeScoreAtHeight(infinitynodePeer.burntx, nSINtypeCanLockReward, lockRewardRequestRet.nRewardHeight - 101, nScore)) {
        LogPrintf("CInfinityNodeLockReward::VerifyMyPeerAndSendVerifyRequest -- Can't calculate score for Infinitynode %s\n",
                    infinitynodePeer.burntx.ToStringShort());
        return false;
    }

    //step 1.2.2: if my score is in Top, i will verify that candidate is online at IP
    //Iam in TopNode => im not expire at Height
    if(nScore <= Params().GetConsensus().nInfinityNodeLockRewardTop) {
        //step 2.1: verify node which send the request is online at IP in metadata
        //SendVerifyRequest()
        //step 2.2: send commitment
        LogPrintf("CInfinityNodeLockReward::VerifyMyPeerAndSendVerifyRequest -- TODO: Iam in TopNode. Sending a VerifyRequest to candidate\n");
    } else {
        LogPrintf("CInfinityNodeLockReward::VerifyMyPeerAndSendVerifyRequest -- TODO: Iam NOT in TopNode. Do nothing!\n");
    }

    //step 1.3 send VerifyRequest.
    // for some reason, IP of candidate is not good in metadata => just return false and dont ban this node
    CService candidateService = metaCandidate.getService();
    if(!SendVerifyRequest(CAddress(candidateService, NODE_NETWORK), infinitynodePeer.burntx, lockRewardRequestRet, connman)){
        LogPrintf("CInfinityNodeLockReward::VerifyMyPeerAndSendVerifyRequest -- Cannot send verify message to candidate %s\n", infCandidate.getBurntxOutPoint().ToStringShort());
        return false;
    }

    return true;
}

/*
 * STEP 1: get a new LockRewardRequest, check it and send verify message
 *
 * STEP 1.3 send verify request to Candidate A at IP(addr) for Request(lockRewardRequestRet)
 */
bool CInfinityNodeLockReward::SendVerifyRequest(const CAddress& addr, COutPoint& myPeerBurnTx, CLockRewardRequest& lockRewardRequestRet, CConnman& connman)
{
    if(fLiteMode || !fInfinityNode) {return false;}

    CVerifyRequest vrequest(addr, myPeerBurnTx, lockRewardRequestRet.burnTxIn.prevout, GetRandInt(999999),
                            lockRewardRequestRet.nRewardHeight, lockRewardRequestRet.GetHash());

    std::string strMessage = strprintf("%s%s%d%s", myPeerBurnTx.ToString(), lockRewardRequestRet.burnTxIn.prevout.ToString(),
                                       vrequest.nonce, lockRewardRequestRet.GetHash().ToString());

    if(!CMessageSigner::SignMessage(strMessage, vrequest.vchSig1, infinitynodePeer.keyInfinitynode)) {
        LogPrintf("CInfinityNodeLockReward::SendVerifyRequest -- SignMessage() failed\n");
        return false;
    }

    std::string strError;

    if(!CMessageSigner::VerifyMessage(infinitynodePeer.pubKeyInfinitynode, vrequest.vchSig1, strMessage, strError)) {
        LogPrintf("CInfinityNodeLockReward::SendVerifyRequest -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    //check if Ive connected to candidate or not
    std::vector<CNode*> vNodesCopy = connman.CopyNodeVector();
    CAddress add = CAddress(addr, NODE_NETWORK);
    bool fconnected = false;
    for (auto* pnode : vNodesCopy)
    {
        if (pnode->addr.ToStringIP() == add.ToStringIP()){
            fconnected = true;
            LogPrintf("CInfinityNodeLockReward::SendVerifyRequest -- verifying node by direct connection nVersion: %d, using nonce %d, addr=%s, Sig1 :%d\n",
                    pnode->GetSendVersion(), vrequest.nonce, addr.ToString(), vrequest.vchSig1.size());
            connman.PushMessage(pnode, CNetMsgMaker(pnode->GetSendVersion()).Make(NetMsgType::INFVERIFY, vrequest));
        }
    }
    // looped through all nodes, release them
    connman.ReleaseNodeVector(vNodesCopy);

    if(!fconnected){
        CNode* pnode = connman.OpenNetworkConnection(add, false, nullptr, NULL, false, false, false, true);
        if(pnode == NULL) {
            LogPrintf("CInfinityNodeLockReward::SendVerifyRequest -- can't connect to node to verify it, addr=%s\n", addr.ToString());
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        LogPrintf("CInfinityNodeLockReward::SendVerifyRequest -- verifying node by direct connection nVersion: %d, using nonce %d, addr=%s, Sig1 :%d\n",
                    pnode->GetSendVersion(), vrequest.nonce, addr.ToString(), vrequest.vchSig1.size());
        connman.PushMessage(pnode, CNetMsgMaker(pnode->GetSendVersion()).Make(NetMsgType::INFVERIFY, vrequest));
    }

    return true;
}

/*
 * STEP 2: get a verify message, check and try to answer the message
 *
 * check:
 * - VerifyRequest was sent from Top Node
 * - Signature of VerifyRequest is correct
 * answer:
 * - create the 2nd signature - my signature and send back to node
 * processVerifyRequestReply:
 * - check 2nd signature
 * - disconnect to node ( Candidate )
 */
//TODO: ban pnode if false
bool CInfinityNodeLockReward::SendVerifyReply(CNode* pnode, CVerifyRequest& vrequest, CConnman& connman)
{
    // only Infinitynode will answer the verify requrest
    if(fLiteMode || !fInfinityNode) {return false;}

    AssertLockHeld(cs);

    //step 2.0 get publicKey of sender and check signature
    infinitynode_info_t infoInf;
    if(!infnodeman.GetInfinitynodeInfo(vrequest.vin1.prevout, infoInf)){
        LogPrintf("CInfinityNodeLockReward::SendVerifyReply -- Cannot find sender from list %s\n");
        //someone try to send a VerifyRequest to me but not in DIN => so ban it
        LOCK(cs_main);
        Misbehaving(pnode->GetId(), 20);
        return false;
    }

    CMetadata metaSender = infnodemeta.Find(infoInf.metadataID);
    if (metaSender.getMetadataHeight() == 0){
        //for some reason, metadata is not updated, do nothing
        LogPrintf("CInfinityNodeLockReward::SendVerifyReply -- Cannot find sender from list %s\n");
        return false;
    }

    std::string metaPublicKey = metaSender.getMetaPublicKey();
    std::vector<unsigned char> tx_data = DecodeBase64(metaPublicKey.c_str());
    CPubKey pubKey(tx_data.begin(), tx_data.end());

    std::string strError;
    std::string strMessage = strprintf("%s%s%d%s", vrequest.vin1.prevout.ToString(), vrequest.vin2.prevout.ToString(),
                                       vrequest.nonce, vrequest.nHashRequest.ToString());
    if(!CMessageSigner::VerifyMessage(pubKey, vrequest.vchSig1, strMessage, strError)) {
        //sender is in DIN and metadata is correct but sign is KO => so ban it
        LogPrintf("CInfinityNodeLockReward::SendVerifyReply -- VerifyMessage() failed, error: %s, message: \n", strError, strMessage);
        LOCK(cs_main);
        Misbehaving(pnode->GetId(), 20);
        return false;
    }

    //step 2.1 check if sender in Top and SINtype = nInfinityNodeLockRewardSINType (skip this step for now)
    int nScore;
    int nSINtypeCanLockReward = Params().GetConsensus().nInfinityNodeLockRewardSINType; //mypeer must be this SINtype, if not, score is NULL

    if(!infnodeman.getNodeScoreAtHeight(vrequest.vin1.prevout, nSINtypeCanLockReward, vrequest.nBlockHeight - 101, nScore)) {
        LogPrintf("CInfinityNodeLockReward::VerifyMyPeerAndSendVerifyRequest -- Can't calculate score for Infinitynode %s\n",
                    infinitynodePeer.burntx.ToStringShort());
        return false;
    }

    //sender in TopNode => he is not expired at Height
    if(nScore <= Params().GetConsensus().nInfinityNodeLockRewardTop) {
        LogPrintf("CInfinityNodeLockReward::VerifyMyPeerAndSendVerifyRequest -- TODO: Someone in TopNode send me a VerifyRequest. Answer him now...\n");
    } else {
        LogPrintf("CInfinityNodeLockReward::VerifyMyPeerAndSendVerifyRequest -- TODO: Someone NOT in TopNode send me a VerifyRequest. Banned!\n");
    }

    //step 2.2 sign a new message and send it back to sender
    vrequest.addr = infinitynodePeer.service;
    std::string strMessage2 = strprintf("%s%d%s%s%s", vrequest.addr.ToString(false), vrequest.nonce, vrequest.nHashRequest.ToString(),
        vrequest.vin1.prevout.ToStringShort(), vrequest.vin2.prevout.ToStringShort());

    if(!CMessageSigner::SignMessage(strMessage2, vrequest.vchSig2, infinitynodePeer.keyInfinitynode)) {
        LogPrintf("CInfinityNodeLockReward::ProcessVerifyReply -- SignMessage() failed\n");
        return false;
    }

    if(!CMessageSigner::VerifyMessage(infinitynodePeer.pubKeyInfinitynode, vrequest.vchSig2, strMessage2, strError)) {
                        LogPrintf("CInfinityNodeLockReward::ProcessVerifyReply -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    connman.PushMessage(pnode, CNetMsgMaker(pnode->GetSendVersion()).Make(NetMsgType::INFVERIFY, vrequest));
    return true;
}

/*
 * STEP 3: check return VerifyRequest, if it is OK then
 * - send the commitment
 * - disconect node
 */
bool CInfinityNodeLockReward::CheckVerifyReply(CNode* pnode, CVerifyRequest& vrequest, CConnman& connman)
{
    // only Infinitynode will answer the verify requrest
    if(fLiteMode || !fInfinityNode) {return false;}

    AssertLockHeld(cs);

    //step 3.0 LockReward height > currentHeight
    if(vrequest.nBlockHeight <= nCachedBlockHeight){
        LogPrintf("CInfinityNodeLockReward::CheckVerifyReply -- nHeight of request %d is inferior than current height %d\n", vrequest.nBlockHeight, nCachedBlockHeight);
        return false;
    }

    //step 3.1 Sig1 from me
    std::string strMessage = strprintf("%s%s%d%s", infinitynodePeer.burntx.ToString(), vrequest.vin2.prevout.ToString(), vrequest.nonce, vrequest.nHashRequest.ToString());
    std::string strError;
    if(!CMessageSigner::VerifyMessage(infinitynodePeer.pubKeyInfinitynode, vrequest.vchSig1, strMessage, strError)) {
        LogPrintf("CInfinityNodeLockReward::CheckVerifyReply -- VerifyMessage(Sig1) failed, error: %s\n", strError);
        return false;
    }

    //step 3.2 Sig2 from Candidate
    std::string strMessage2 = strprintf("%s%d%s%s%s", vrequest.addr.ToString(false), vrequest.nonce, vrequest.nHashRequest.ToString(),
        vrequest.vin1.prevout.ToStringShort(), vrequest.vin2.prevout.ToStringShort());

    infinitynode_info_t infoInf;
    if(!infnodeman.GetInfinitynodeInfo(vrequest.vin2.prevout, infoInf)){
        LogPrintf("CInfinityNodeLockReward::CheckVerifyReply -- Cannot find sender from list %s\n");
        return false;
    }

    CMetadata metaCandidate = infnodemeta.Find(infoInf.metadataID);
    if (metaCandidate.getMetadataHeight() == 0){
        LogPrintf("CInfinityNodeLockReward::CheckVerifyReply -- Cannot find sender from list %s\n");
        return false;
    }

    std::string metaPublicKey = metaCandidate.getMetaPublicKey();
    std::vector<unsigned char> tx_data = DecodeBase64(metaPublicKey.c_str());
    CPubKey pubKey(tx_data.begin(), tx_data.end());

    if(!CMessageSigner::VerifyMessage(pubKey, vrequest.vchSig2, strMessage2, strError)){
        LogPrintf("CInfinityNodeLockReward::CheckVerifyReply -- VerifyMessage(Sig2) failed, error: %s\n", strError);
        return false;
    }

    //step 3.3 send commitment
    if(!SendCommitment(vrequest.nHashRequest, connman)){
        LogPrintf("CInfinityNodeLockReward::CheckVerifyReply -- Cannot send commitment\n", strError);
        return false;
    }

    //pnode->fDisconnect = true;
    return true;
}

/*
 * STEP 4: Create, Relay, Update the commitments
 *
 * STEP 4.1 create/send/relay commitment
 * control if i am candidate and how many commitment receive to decide MuSig workers
 */
bool CInfinityNodeLockReward::SendCommitment(const uint256& reqHash, CConnman& connman)
{
    if(fLiteMode || !fInfinityNode) return false;

    AssertLockHeld(cs);

    CKey secret;
    secret.MakeNewKey(true);

    CLockRewardCommitment commitment(reqHash, infinitynodePeer.burntx, secret);
    if(commitment.Sign(infinitynodePeer.keyInfinitynode, infinitynodePeer.pubKeyInfinitynode)) {
        if(AddCommitment(commitment)){
            LogPrintf("CInfinityNodeLockReward::SendCommitment -- Send my commitment %s\n", commitment.GetHash().ToString());
            commitment.Relay(connman);
            return true;
        }
    }
    return false;
}

void CInfinityNodeLockReward::AddMySignersMap(const CLockRewardCommitment& commitment)
{
    if(fLiteMode || !fInfinityNode) return;

    AssertLockHeld(cs);

    if(commitment.nHashRequest != currentLockRequestHash){
        LogPrintf("CInfinityNodeLockReward::AddMySignersMap -- commitment is not mine. LockRequest hash: %s\n", commitment.nHashRequest.ToString());
        return;
    }

    auto it = mapSigners.find(currentLockRequestHash);
    if(it == mapSigners.end()){
        mapSigners[currentLockRequestHash].push_back(commitment.vin.prevout);
        LogPrintf("CInfinityNodeLockReward::AddMySignersMap -- add commitment to my signer map(%d): %s\n",
                   mapSigners[currentLockRequestHash].size(),commitment.vin.prevout.ToStringShort());
    } else {
        bool found=false;
        for (auto& v : it->second){
            if(v == commitment.vin.prevout){
                found = true;
                LogPrintf("CInfinityNodeLockReward::AddMySignersMap -- commitment from same signer: %s\n", commitment.vin.prevout.ToStringShort());
            }
        }
        if(!found){
            LogPrintf("CInfinityNodeLockReward::AddMySignersMap -- update commitment to my signer map(%d): %s\n",
                       mapSigners[currentLockRequestHash].size(),commitment.vin.prevout.ToStringShort());
            mapSigners[currentLockRequestHash].push_back(commitment.vin.prevout);
        }
    }
}

/*
 * STEP 4.3
 *
 * read commitment map and myLockRequest, if there is enough commitment was sent
 * => broadcast it
 */
bool CInfinityNodeLockReward::FindAndSendSignersGroup()
{
    if(fLiteMode || !fInfinityNode) return false;

    AssertLockHeld(cs);

    int loop = Params().GetConsensus().nInfinityNodeCallLockRewardDeepth / Params().GetConsensus().nInfinityNodeCallLockRewardLoop;

    for (int i=0; i <= loop; i++)
    {
        if(i >=1 && mapSigners.size() >= Params().GetConsensus().nInfinityNodeCallLockRewardLoop * i && nGroupSigners < i){
            std::vector<COutPoint> signers;
            for(int j=Params().GetConsensus().nInfinityNodeCallLockRewardLoop * (i - 1); j < Params().GetConsensus().nInfinityNodeCallLockRewardLoop * i; j++){
                signers.push_back(mapSigners[currentLockRequestHash].at(j));
            }

            if(signers.size() == Params().GetConsensus().nInfinityNodeCallLockRewardLoop){
                nGroupSigners = i;//track signer group sent
                int nSINtypeCanLockReward = Params().GetConsensus().nInfinityNodeLockRewardSINType;
                std::string signerIndex = infnodeman.getVectorNodeScoreAtHeight(signers, nSINtypeCanLockReward, mapLockRewardRequest[currentLockRequestHash].nRewardHeight);
                LogPrintf("CInfinityNodeLockReward::FindAndSendSignersGroup -- send this group: %d, signers: %s\n", nGroupSigners, signerIndex);
                return true;
            }
        }
    }

    return false;
}

/*
 * STEP 0: create LockRewardRequest if i am a candidate at nHeight
 */
bool CInfinityNodeLockReward::ProcessBlock(int nBlockHeight, CConnman& connman)
{
    if(fLiteMode || !fInfinityNode) return false;
    //DIN must be built before begin the process
    if(infnodeman.getMapStatus() == false) return false;
    //mypeer must have status STARTED
    if(infinitynodePeer.nState != INFINITYNODE_PEER_STARTED) return false;

    //step 0.1: Check if this InfinitynodePeer is a candidate at nBlockHeight
    CInfinitynode infRet;
    if(!infnodeman.Get(infinitynodePeer.burntx, infRet)){
        LogPrintf("CInfinityNodeLockReward::ProcessBlock -- Can not identify mypeer in list, height: %d\n", nBlockHeight);
        return false;
    }

    int nRewardHeight = infnodeman.isPossibleForLockReward(infRet.getCollateralAddress());
    if(nRewardHeight == 0 || (nRewardHeight < (nCachedBlockHeight + Params().GetConsensus().nInfinityNodeCallLockRewardLoop))){
        LogPrintf("CInfinityNodeLockReward::ProcessBlock -- Try to LockReward false at height %d\n", nBlockHeight);
        mapSigners.clear();
        currentLockRequestHash = uint256();
        nFutureRewardHeight = 0;
        nGroupSigners = 0;
        return false;
    }

    //step 0.2 we know that nRewardHeight >= nCachedBlockHeight + Params().GetConsensus().nInfinityNodeCallLockRewardLoop
    int loop = (Params().GetConsensus().nInfinityNodeCallLockRewardDeepth / (nRewardHeight - nCachedBlockHeight)) - 1;
    CLockRewardRequest newRequest(nRewardHeight, infRet.getBurntxOutPoint(), infRet.getSINType(), loop);
    if (newRequest.Sign(infinitynodePeer.keyInfinitynode, infinitynodePeer.pubKeyInfinitynode)) {
        LOCK(cs);
        if (AddLockRewardRequest(newRequest)) {
            //track my last request
            mapSigners.clear();
            currentLockRequestHash = newRequest.GetHash();
            nFutureRewardHeight = newRequest.nRewardHeight;
            nGroupSigners = 0;
            //relay it
            LogPrintf("CInfinityNodeLockReward::ProcessBlock -- relay my LockRequest loop: %d, hash: %s\n", loop, newRequest.GetHash().ToString());
            newRequest.Relay(connman);
            return true;
        }
    }
    return true;
}

void CInfinityNodeLockReward::ProcessDirectMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, CConnman& connman)
{
    if(fLiteMode  || !fInfinityNode) return;

    if (strCommand == NetMsgType::INFVERIFY) {
        CVerifyRequest vrequest;
        vRecv >> vrequest;
        LogPrintf("CInfinityNodeLockReward::ProcessDirectMessage -- new VerifyRequest from %d, Sig1: %d, Sig2: %d, hash: %s\n",
                     pfrom->GetId(), vrequest.vchSig1.size(), vrequest.vchSig2.size(), vrequest.GetHash().ToString());
        pfrom->setAskFor.erase(vrequest.GetHash());
        {
            LOCK(cs);
            if(vrequest.vchSig1.size() > 0 &&  vrequest.vchSig2.size() == 0) {
                LogPrintf("CInfinityNodeLockReward::ProcessDirectMessage -- VerifyRequest: I am candidate. Reply the verify from: %d, hash: %s\n",
                          pfrom->GetId(), vrequest.GetHash().ToString());
                SendVerifyReply(pfrom, vrequest, connman);
            } else if(vrequest.vchSig1.size() > 0 &&  vrequest.vchSig2.size() > 0) {
                LogPrintf("CInfinityNodeLockReward::ProcessDirectMessage -- VerifyRequest: I am TopNode. Receive a reply from candidate %d, hash: %s\n",
                          pfrom->GetId(), vrequest.GetHash().ToString());
                if(CheckVerifyReply(pfrom, vrequest, connman)){
                    LogPrintf("CInfinityNodeLockReward::ProcessDirectMessage -- Candidate is valid. Broadcast the my Rpubkey for Musig and disconnect the direct connect to candidata\n");
                }else{
                    LogPrintf("CInfinityNodeLockReward::ProcessDirectMessage -- Candidate is NOT valid.\n");
                }
            }
            return;
        }
    }
}

void CInfinityNodeLockReward::ProcessMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, CConnman& connman)
{
    if(fLiteMode  || !fInfinityNode) return; // disable all SIN specific functionality

    if (strCommand == NetMsgType::INFLOCKREWARDINIT) {
        CLockRewardRequest lockReq;
        vRecv >> lockReq;
        //dont ask pfrom for this Request anymore
        uint256 nHash = lockReq.GetHash();
        pfrom->setAskFor.erase(nHash);
        {
            LOCK(cs);
            if(mapLockRewardRequest.count(nHash)) return;
            if(!CheckLockRewardRequest(pfrom, lockReq, connman, nCachedBlockHeight)){
                return;
            }
            LogPrintf("CInfinityNodeLockReward::ProcessMessage -- receive and add new LockRewardRequest from %d\n",pfrom->GetId());
            if(AddLockRewardRequest(lockReq)){
                lockReq.Relay(connman);
                if(!VerifyMyPeerAndSendVerifyRequest(pfrom, lockReq, connman)){
                    LogPrintf("CInfinityNodeLockReward::ProcessMessage -- VerifyMyPeerAndSendVerifyRequest is false.\n");
                    return;
                }
            }
            return;
        }
    } else if (strCommand == NetMsgType::INFCOMMITMENT) {
        CLockRewardCommitment commitment;
        vRecv >> commitment;
        uint256 nHash = commitment.GetHash();
        pfrom->setAskFor.erase(nHash);
        {
            LOCK(cs);
            if(mapLockRewardCommitment.count(nHash)){
                LogPrintf("CInfinityNodeLockReward::ProcessMessage -- I had it. end process\n");
                return;
            }
            if(AddCommitment(commitment)){
                LogPrintf("CInfinityNodeLockReward::ProcessMessage -- relay Commitment for LockRequest %s. Remind nFutureRewardHeight: %d, LockRequest: %s\n",
                           commitment.nHashRequest.ToString(), nFutureRewardHeight, currentLockRequestHash.ToString());
                commitment.Relay(connman);
                AddMySignersMap(commitment);
                FindAndSendSignersGroup();
            } else {
                LogPrintf("CInfinityNodeLockReward::ProcessMessage -- Cannot relay commitment in network\n");
            }
            return;
        }
    } else if (strCommand == NetMsgType::INFLRMUSIG) {
    }
}

void CInfinityNodeLockReward::UpdatedBlockTip(const CBlockIndex *pindex, CConnman& connman)
{
    if(!pindex) return;

    nCachedBlockHeight = pindex->nHeight;
    LogPrintf("CInfinityNodeLockReward::UpdatedBlockTip -- nCachedBlockHeight=%d\n", nCachedBlockHeight);

    int nFutureBlock = nCachedBlockHeight + Params().GetConsensus().nInfinityNodeCallLockRewardDeepth;

    //CheckPreviousBlockVotes(nFutureBlock);
    ProcessBlock(nFutureBlock, connman);
}
