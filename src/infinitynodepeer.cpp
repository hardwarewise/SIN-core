// Copyright (c) 2018-2019 SIN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <infinitynode.h>
#include <infinitynodepeer.h>
#include <infinitynodeman.h>
#include <util.h>
#include <logging.h>
#include <utilstrencodings.h>

// Declaration of global variable infinitynodePeer with initial value
CInfinitynodePeer infinitynodePeer;

void CInfinitynodePeer::ManageState(CConnman& connman)
{
    LogPrint(BCLog::MASTERNODE, "CActiveMasternode::ManageState -- Start\n");
    if(!fMasterNode) {
        LogPrint(BCLog::MASTERNODE, "CActiveMasternode::ManageState -- Not a masternode, returning\n");
        return;
    }

    if(eType == INFINITYNODE_UNKNOWN) {
        ManageStateInitial(connman);
    }

    if(eType == INFINITYNODE_REMOTE) {
        ManageStateRemote();
    }

    AutoCheck(connman);
}

std::string CInfinitynodePeer::GetStateString() const
{
    switch (nState) {
        case INFINITYNODE_PEER_INITIAL:         return "INITIAL";
        case INFINITYNODE_PEER_INPUT_TOO_NEW:   return "INPUT_TOO_NEW";
        case INFINITYNODE_PEER_NOT_CAPABLE:     return "NOT_CAPABLE";
        case INFINITYNODE_PEER_STARTED:         return "STARTED";
        default:                                return "UNKNOWN";
    }
}

std::string CInfinitynodePeer::GetStatus() const
{
    switch (nState) {
        case INFINITYNODE_PEER_INITIAL:         return "Node just started, not yet activated";
        case INFINITYNODE_PEER_INPUT_TOO_NEW:   return strprintf("Infinitynode input must have at least %d confirmations", Params().MaxReorganizationDepth());
        case INFINITYNODE_PEER_NOT_CAPABLE:     return "Not capable Infinitynode: " + strNotCapableReason;
        case INFINITYNODE_PEER_STARTED:         return "Infinitynode successfully started";
        default:                                return "Unknown";
    }
}

std::string CInfinitynodePeer::GetTypeString() const
{
    std::string strType;
    switch(eType) {
    case INFINITYNODE_REMOTE:
        strType = "REMOTE";
        break;
    default:
        strType = "UNKNOWN";
        break;
    }
    return strType;
}

std::string CInfinitynodePeer::GetMyPeerInfo() const
{
    std::string myPeerInfo;
    infinitynode_info_t infoInf;
    if(eType == INFINITYNODE_UNKNOWN)
    {
        myPeerInfo = strprintf("Can not initial Peer. Please make sure that the configuration: flisten, externalip, network port...");
        return myPeerInfo;
    }
    if(nState != INFINITYNODE_PEER_STARTED)
    {
        return GetStatus();
    }
    if(infnodeman.GetInfinitynodeInfo(HexStr(pubKeyInfinitynode), infoInf) && eType == INFINITYNODE_REMOTE && nState == INFINITYNODE_PEER_STARTED) {
        myPeerInfo = strprintf("My Peer is running at %s\n", infoInf.metadataService.ToString());
    } else {
        myPeerInfo = strprintf("Peer is not ready. Please update the metadata of Infinitynode.");
    }
    return myPeerInfo;
}

bool CInfinitynodePeer::AutoCheck(CConnman& connman)
{
    if(!fPingerEnabled) {
        LogPrintf("CInfinitynodePeer::AutoCheck -- %s: infinitynode ping service is disabled, skipping...\n", GetStateString());
        return false;
    }

    infinitynode_info_t infoInf;
    if(!infnodeman.GetInfinitynodeInfo(HexStr(pubKeyInfinitynode), infoInf))
    {
        strNotCapableReason = "Cannot find the Peer's Key in Deterministic node list";
        nState = INFINITYNODE_PEER_NOT_CAPABLE;
        LogPrintf("CInfinitynodePeer::AutoCheck -- %s\n",  strNotCapableReason);
        return false;
    }
}

void CInfinitynodePeer::ManageStateInitial(CConnman& connman)
{
    LogPrintf("CInfinitynodePeer::ManageStateInitial -- status = %s, type = %s, pinger enabled = %d\n", GetStatus(), GetTypeString(), fPingerEnabled);

    // Check that our local network configuration is correct
    if (!fListen) {
        // listen option is probably overwritten by smth else, no good
        nState = INFINITYNODE_PEER_NOT_CAPABLE;
        strNotCapableReason = "Infinitynode must accept connections from outside. Make sure listen configuration option is not overwritten by some another parameter.";
        LogPrintf("CInfinitynodePeer::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    // First try to find whatever local address is specified by externalip option
    bool fFoundLocal = GetLocal(service) && CInfinitynode::IsValidNetAddr(service);
    if(!fFoundLocal) {
        bool empty = true;
        // If we have some peers, let's try to find our local address from one of them
        connman.ForEachNodeContinueIf(CConnman::AllNodes, [&fFoundLocal, &empty, this](CNode* pnode) {
            empty = false;
            if (pnode->addr.IsIPv4())
                fFoundLocal = GetLocal(service, &pnode->addr) && CInfinitynode::IsValidNetAddr(service);
            return !fFoundLocal;
        });
        // nothing and no live connections, can't do anything for now
        if (empty) {
            nState = INFINITYNODE_PEER_NOT_CAPABLE;
            strNotCapableReason = "Can't detect valid external address. Will retry when there are some connections available.";
            LogPrintf("CInfinitynodePeer::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
    }

    if(!fFoundLocal) {
        nState = INFINITYNODE_PEER_NOT_CAPABLE;
        strNotCapableReason = "Can't detect valid external address. Please consider using the externalip configuration option if problem persists. Make sure to use IPv4 address only.";
        LogPrintf("CInfinitynodePeer::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    const auto defaultChainParams = CreateChainParams(CBaseChainParams::MAIN);
    int mainnetDefaultPort = defaultChainParams->GetDefaultPort();
    if(Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if(service.GetPort() != mainnetDefaultPort) {
            nState = INFINITYNODE_PEER_NOT_CAPABLE;
            strNotCapableReason = strprintf("Invalid port: %u - only %d is supported on mainnet.", service.GetPort(), mainnetDefaultPort);
            LogPrintf("CInfinitynodePeer::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
    } else if(service.GetPort() == mainnetDefaultPort) {
        nState = INFINITYNODE_PEER_NOT_CAPABLE;
        strNotCapableReason = strprintf("Invalid port: %u - %d is only supported on mainnet.", service.GetPort(), mainnetDefaultPort);
        LogPrintf("CInfinitynodePeer::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    LogPrintf("CInfinitynodePeer::ManageStateInitial -- Checking inbound connection to '%s'\n", service.ToString());

    bool fConnected = false;
    SOCKET hSocket = CreateSocket(service);
    if (hSocket != INVALID_SOCKET) {
        fConnected = ConnectSocketDirectly(service, hSocket, nConnectTimeout, true) && IsSelectableSocket(hSocket);
        CloseSocket(hSocket);
    }

    if (!fConnected) {
        nState = INFINITYNODE_PEER_NOT_CAPABLE;
        strNotCapableReason = "Could not connect to " + service.ToString();
        LogPrintf("CInfinitynodePeer::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    // Default to REMOTE
    eType = INFINITYNODE_REMOTE;

    LogPrintf("CInfinitynodePeer::ManageStateInitial -- End status = %s, type = %s, pinger enabled = %d\n", GetStatus(), GetTypeString(), fPingerEnabled);
}

void CInfinitynodePeer::ManageStateRemote()
{
    LogPrintf("CInfinitynodePeer::ManageStateRemote -- Start status = %s, type = %s, pinger enabled = %d, pubKeyInfinitynode.GetID() = %s\n", 
             GetStatus(), GetTypeString(), fPingerEnabled, pubKeyInfinitynode.GetID().ToString());

    infinitynode_info_t infoInf;
    if(infnodeman.GetInfinitynodeInfo(HexStr(pubKeyInfinitynode), infoInf)) {
        if(infoInf.nProtocolVersion != PROTOCOL_VERSION) {
            nState = INFINITYNODE_PEER_NOT_CAPABLE;
            strNotCapableReason = "Invalid protocol version";
            LogPrintf("CInfinitynodePeer::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
        if(service != infoInf.metadataService) {
            nState = INFINITYNODE_PEER_NOT_CAPABLE;
            strNotCapableReason = "Broadcasted IP doesn't match our external address. Make sure you issued a new broadcast if IP of this Infinitynode changed recently.";
            LogPrintf("CInfinitynodePeer::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
        if(!CInfinitynode::IsValidStateForAutoStart(infoInf.nMetadataHeight)) {
            nState = INFINITYNODE_PEER_NOT_CAPABLE;
            strNotCapableReason = strprintf("Infinitynode metadata height is %d", infoInf.nMetadataHeight);
            LogPrintf("CInfinitynodePeer::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
        if(nState != INFINITYNODE_PEER_STARTED) {
            LogPrintf("CInfinitynodePeer::ManageStateRemote -- STARTED!\n");
            burntx = infoInf.vinBurnFund.prevout; //initial value
            service = infoInf.metadataService;
            fPingerEnabled = true;
            nSINType = infoInf.nSINType;
            nState = INFINITYNODE_PEER_STARTED;
        }
    }
    else {
        nState = INFINITYNODE_PEER_NOT_CAPABLE;
        strNotCapableReason = "Infinitynode is not in Deterministic Infinitynode list";
        LogPrintf("CInfinitynodePeer::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
    }
}