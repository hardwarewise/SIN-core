// Copyright (c) 2018-2019 SIN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SIN_INFINITYNODELOCKREWARD_H
#define SIN_INFINITYNODELOCKREWARD_H

#include <infinitynodeman.h>

class CInfinityNodeLockReward;
class CLockRewardRequest;

extern CInfinityNodeLockReward inflockreward;

class CLockRewardRequest
{
public:
    int nRewardHeight{0};
    CTxIn burnTxIn{};
    int nSINtype{0};
    int nLoop{0};
    std::vector<unsigned char> vchSig{};
    bool fFinal = false;

    CLockRewardRequest();
    CLockRewardRequest(int Height, COutPoint burnTxIn, int nSINtype, int loop = 0);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nRewardHeight);
        READWRITE(burnTxIn);
        READWRITE(nSINtype);
        READWRITE(nLoop);
        READWRITE(vchSig);
        READWRITE(fFinal);
    }

    uint256 GetHash() const
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << nRewardHeight;
        ss << burnTxIn;
        ss << nSINtype;
        ss << nLoop;
        return ss.GetHash();
    }

    bool Sign(const CKey& keyInfinitynode, const CPubKey& pubKeyInfinitynode);
    bool CheckSignature(CPubKey& pubKeyInfinitynode, int &nDos);
    bool IsValid(CNode* pnode, int nValidationHeight, std::string& strError, CConnman& connman);
    void Relay(CConnman& connman);
};

class CInfinityNodeLockReward
{
private:

    mutable CCriticalSection cs;
    std::map<uint256, CLockRewardRequest> mapLockRewardRequest;
    std::map<uint256, int> mapInfinityNodeCommitment;
    std::map<uint256, int> mapInfinityNodeRandom;
    // Keep track of current block height
    int nCachedBlockHeight;

public:

    CInfinityNodeLockReward() : nCachedBlockHeight(0) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(mapLockRewardRequest);
    }

    void Clear();

    bool AlreadyHave(const uint256& hash);

    /*TODO: LockRewardRequest*/
    bool AddLockRewardRequest(const CLockRewardRequest& lockRewardRequest);
    void RemoveLockRewardRequest(const CLockRewardRequest& lockRewardRequest);
    bool HasLockRewardRequest(const uint256& reqHash);
    bool GetLockRewardRequest(const uint256& reqHash, CLockRewardRequest& lockRewardRequestRet);

    /*TODO: Verify node at IP*/
    /*
    bool SendVerifyRequest(const CAddress& addr, const std::vector<CInfinitynode*>& vSortedByAddr, CConnman& connman);
    void SendVerifyReply(CNode* pnode, CMasternodeVerification& mnv, CConnman& connman);
    void ProcessVerifyReply(CNode* pnode, CMasternodeVerification& mnv);
    void ProcessVerifyBroadcast(CNode* pnode, const CMasternodeVerification& mnv);
    */

    //process consensus request message
    bool ProcessRewardLockRequest(CNode* pfrom, CLockRewardRequest& lockRewardRequestRet, CConnman& connman, int nBlockHeight);
    /*call in UpdatedBlockTip*/
    bool ProcessBlock(int nBlockHeight, CConnman& connman);
    /*call in processing.cpp when node receive INV*/
    void ProcessMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, CConnman& connman);
    /*call in dsnotificationinterface.cpp when node connect a new block*/
    void UpdatedBlockTip(const CBlockIndex *pindex, CConnman& connman);
};
#endif // SIN_INFINITYNODELOCKREWARD_H