// Copyright (c) 2018-2019 SIN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SIN_INFINITYNODELOCKREWARD_H
#define SIN_INFINITYNODELOCKREWARD_H

#include <infinitynodeman.h>

class CInfinityNodeLockReward;
class CLockRewardRequest;
class CVerifyRequest;
class CLockRewardCommitment;

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

class CVerifyRequest
{
public:
    CTxIn vin1{};
    CTxIn vin2{};
    CService addr{};
    int nonce{};
    int nBlockHeight{};
    uint256 nHashRequest{};
    std::vector<unsigned char> vchSig1{};
    std::vector<unsigned char> vchSig2{};

    CVerifyRequest() = default;

    CVerifyRequest(CService addrToConnect, COutPoint myPeerBurnTxIn, COutPoint candidateBurnTxIn, int nonce, int nBlockHeight, uint256 nRequest) :
        vin1(CTxIn(myPeerBurnTxIn)),
        vin2(CTxIn(candidateBurnTxIn)),
        addr(addrToConnect),
        nonce(nonce),
        nBlockHeight(nBlockHeight),
        nHashRequest(nRequest)
    {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(vin1);
        READWRITE(vin2);
        READWRITE(addr);
        READWRITE(nonce);
        READWRITE(nBlockHeight);
        READWRITE(nHashRequest);
        READWRITE(vchSig1);
        READWRITE(vchSig2);
    }

    uint256 GetHash() const
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << vin1;
        ss << vin2;
        ss << addr;
        ss << nonce;
        ss << nBlockHeight;
        ss << nHashRequest;
        return ss.GetHash();
    }

    void Relay() const
    {
        CInv inv(MSG_INFVERIFY, GetHash());
        g_connman->RelayInv(inv);
    }
};

class CLockRewardCommitment
{
public:
    CTxIn vin{};//Top node ID
    uint256 nHashRequest;//LockRewardRequest hash
    int nonce{};
    std::vector<unsigned char> vchSig{};
    CKey random;//r of schnorr Musig
    CPubKey pubkeyR;

    CLockRewardCommitment();
    CLockRewardCommitment(uint256 nRequest, CKey key);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(vin);
        READWRITE(nHashRequest);
        READWRITE(pubkeyR);
        READWRITE(nonce);
        READWRITE(vchSig);
    }

    uint256 GetHash() const
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << vin;
        ss << nHashRequest;
        ss << pubkeyR;
        ss << nonce;
        return ss.GetHash();
    }

    bool Sign(const CKey& keyInfinitynode, const CPubKey& pubKeyInfinitynode);
    bool CheckSignature(CPubKey& pubKeyInfinitynode, int &nDos);
    void Relay(CConnman& connman);
};

class CInfinityNodeLockReward
{
private:

    mutable CCriticalSection cs;
    std::map<uint256, CLockRewardRequest> mapLockRewardRequest;
    std::map<uint256, CLockRewardCommitment> mapLockRewardCommitment;
    // Keep track of current block height
    int nCachedBlockHeight;

public:

    CInfinityNodeLockReward() : nCachedBlockHeight(0) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(mapLockRewardRequest);
        READWRITE(mapLockRewardCommitment);
    }

    void Clear();

    bool AlreadyHave(const uint256& hash);

    //LockRewardRequest
    bool AddLockRewardRequest(const CLockRewardRequest& lockRewardRequest);
    void RemoveLockRewardRequest(const CLockRewardRequest& lockRewardRequest);
    bool HasLockRewardRequest(const uint256& reqHash);
    bool GetLockRewardRequest(const uint256& reqHash, CLockRewardRequest& lockRewardRequestRet);

    //process consensus request message
    bool getCkeyForRequest(uint256 nRequest);
    bool CheckLockRewardRequest(CNode* pfrom, CLockRewardRequest& lockRewardRequestRet, CConnman& connman, int nBlockHeight);
    bool VerifyLockRewardCandidate(CLockRewardRequest& lockRewardRequestRet, CConnman& connman);

    //Verify node at IP
    bool SendVerifyRequest(const CAddress& addr, COutPoint& myPeerBurnTx, CLockRewardRequest& lockRewardRequestRet, CConnman& connman);
    bool SendVerifyReply(CNode* pnode, CVerifyRequest& vrequest, CConnman& connman);
    bool CheckVerifyReply(CNode* pnode, CVerifyRequest& vrequest, CConnman& connman);

    //commitment
    bool AddCommitment(const CLockRewardCommitment& commitment);
    bool SendCommitment(const uint256& reqHash, CConnman& connman);
    bool VerifyCommitment(const uint256& reqHash, const CLockRewardCommitment& commitment);

    //call in UpdatedBlockTip
    bool ProcessBlock(int nBlockHeight, CConnman& connman);
    //call in processing.cpp when node receive INV
    void ProcessMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, CConnman& connman);
    //call in dsnotificationinterface.cpp when node connect a new block
    void UpdatedBlockTip(const CBlockIndex *pindex, CConnman& connman);
};
#endif // SIN_INFINITYNODELOCKREWARD_H