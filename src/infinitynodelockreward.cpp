// Copyright (c) 2018-2019 SIN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <infinitynodelockreward.h>
#include <infinitynodeman.h>
#include <infinitynodepeer.h>
#include <messagesigner.h>

#include <boost/lexical_cast.hpp>

/** Object for who's going to get paid on which blocks */
CInfinityNodeLockReward inflockreward;

/*************************************************************/
/***** CLockRewardRequest ************************************/
/*************************************************************/
CLockRewardRequest::CLockRewardRequest(int Height, COutPoint outpoint, int sintype, int loop){
    nRewardHeight = Height;
    burnTxIn = CTxIn(outpoint);
    nSINtype = sintype;
    nLoop = loop;
    sigTime = GetAdjustedTime();
}

bool CLockRewardRequest::Sign(const CKey& keyInfinitynode, const CPubKey& pubKeyInfinitynode)
{
    std::string strError;
    std::string strSignMessage;

    std::string strMessage = boost::lexical_cast<std::string>(nRewardHeight) + burnTxIn.ToString()
                             + boost::lexical_cast<std::string>(nSINtype)
                             + boost::lexical_cast<std::string>(nLoop)
                             + boost::lexical_cast<std::string>(sigTime);

    if(!CMessageSigner::SignMessage(strMessage, vchSig, keyInfinitynode)) {
        LogPrintf("CLockRewardRequest::Sign -- SignMessage() failed\n");
        return false;
    }

    if(!CMessageSigner::VerifyMessage(pubKeyInfinitynode, vchSig, strMessage, strError)) {
        LogPrintf("CMasternodePing::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CLockRewardRequest::CheckSignature(CPubKey& pubKeyInfinitynode)
{
    std::string strMessage = boost::lexical_cast<std::string>(nRewardHeight) + burnTxIn.ToString()
                             + boost::lexical_cast<std::string>(nSINtype)
                             + boost::lexical_cast<std::string>(nLoop)
                             + boost::lexical_cast<std::string>(sigTime);
    std::string strError = "";

    if(!CMessageSigner::VerifyMessage(pubKeyInfinitynode, vchSig, strMessage, strError)) {
        LogPrintf("CMasternodePing::CheckSignature -- Got bad Infinitynode LockReward signature, ID=%s, error: %s\n", burnTxIn.prevout.ToStringShort(), strError);
        return false;
    }
    return true;
}

bool CLockRewardRequest::SimpleCheck()
{
    if (sigTime > GetAdjustedTime() + 60 * 60) {
        LogPrintf("CLockRewardRequest::SimpleCheck -- Signature rejected, too far into the future, ID=%s\n", burnTxIn.prevout.ToStringShort());
        return false;
    }

    {
        CInfinitynode inf;
        infnodeman.deterministicRewardAtHeight(nRewardHeight, nSINtype, inf);
        if(inf.vinBurnFund != burnTxIn){return false;}
    }
    return true;
}

void CLockRewardRequest::Relay(CConnman& connman)
{

    CInv inv(MSG_LOCKREWARD_INIT, GetHash());
    connman.RelayInv(inv);
}

/*************************************************************/
/***** CInfinityNodeLockReward *******************************/
/*************************************************************/
void CInfinityNodeLockReward::Clear()
{
    LOCK(cs);
    mapLockRewardRequest.clear();
    mapInfinityNodeRandom.clear();
    mapInfinityNodeCommitment.clear();
}

bool CInfinityNodeLockReward::AlreadyHave(const uint256& hash)
{
    LOCK(cs);
    return mapLockRewardRequest.count(hash) ||
            mapInfinityNodeRandom.count(hash) ||
            mapInfinityNodeCommitment.count(hash);
}

void CInfinityNodeLockReward::AddLockRewardRequest(const CLockRewardRequest& lockRewardRequest)
{
    LogPrintf("CInfinityNodeLockReward::AddLockRewardRequest -- lockRewardRequest hash %s\n", lockRewardRequest.GetHash().ToString());

    if(lockRewardRequest.nLoop == 0){
        {
            LOCK(cs);
            mapLockRewardRequest.insert(make_pair(lockRewardRequest.GetHash(), lockRewardRequest));
        }
    } else if (lockRewardRequest.nLoop > 0){
        //new request from candidate, so remove old requrest
        RemoveLockRewardRequest(lockRewardRequest);
        {
            LOCK(cs);
            mapLockRewardRequest.insert(make_pair(lockRewardRequest.GetHash(), lockRewardRequest));
        }
    }
}

/*
 * find old request which has the same nRewardHeight, burnTxIn, nSINtype, nLoop inferior => remove it
 */
void CInfinityNodeLockReward::RemoveLockRewardRequest(const CLockRewardRequest& lockRewardRequest)
{
    LOCK(cs);
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

bool CInfinityNodeLockReward::ProcessBlock(int nBlockHeight, CConnman& connman)
{
    if(fLiteMode || !fInfinityNode) return false;

    //step 1: Check if this InfinitynodePeer is a candidate at nBlockHeight
    CInfinitynode infRet;
    if(!infnodeman.deterministicRewardAtHeight(nBlockHeight, infinitynodePeer.nSINType, infRet)){
        LogPrintf("CInfinityNodeLockReward::ProcessBlock -- Can not identify the candidate at height %d\n", nBlockHeight);
        return false;
    }

    if(infinitynodePeer.burntx != infRet.getBurntxOutPoint()){
        LogPrintf("CInfinityNodeLockReward::ProcessBlock -- This peer is not candidate at height %d\n", nBlockHeight);
        return false;
    }

    //step 2: send INV to lock the reward for this InfinityNode
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << infinitynodePeer.burntx;
    CInv inv(MSG_LOCKREWARD_INIT, ss.GetHash());
    connman.RelayInv(inv);

    return true;
}

void CInfinityNodeLockReward::UpdatedBlockTip(const CBlockIndex *pindex, CConnman& connman)
{
    if(!pindex) return;

    nCachedBlockHeight = pindex->nHeight;
    LogPrintf("CInfinityNodeLockReward::UpdatedBlockTip -- nCachedBlockHeight=%d\n", nCachedBlockHeight);

    int nFutureBlock = nCachedBlockHeight + LOCKREWARD_AT_FUTURE;

    //CheckPreviousBlockVotes(nFutureBlock);
    ProcessBlock(nFutureBlock, connman);
}
