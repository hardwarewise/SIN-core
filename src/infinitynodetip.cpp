// Copyright (c) 2018-2019 SIN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <infinitynodetip.h>
#include <infinitynodeman.h>
#include <checkpoints.h>
#include <validation.h>
#include <ui_interface.h>
#include <util.h>

CInfinitynodeTip infTip;

CInfinitynodeTip::CInfinitynodeTip()
: fFinished(false)
{}

void CInfinitynodeTip::UpdatedBlockTip(const CBlockIndex *pindexNew, bool fInitialDownload, CConnman& connman)
{
    LogPrintf("CInfinitynodeTip::UpdatedBlockTip -- pindexNew->nHeight: %d fInitialDownload=%d\n", pindexNew->nHeight, fInitialDownload);
    if (fInitialDownload) {return;}

    static bool fReachedBestHeader = false;
    bool fReachedBestHeaderNew = pindexNew->GetBlockHash() == pindexBestHeader->GetBlockHash();

    if (fReachedBestHeader && !fReachedBestHeaderNew) {
        fReachedBestHeader = false;
        return;
    }

    fReachedBestHeader = fReachedBestHeaderNew;

    LogPrintf("CInfinitynodeTip::UpdatedBlockTip -- pindexNew->nHeight: %d pindexBestHeader->nHeight: %d fInitialDownload=%d fReachedBestHeader=%d\n",
                pindexNew->nHeight, pindexBestHeader->nHeight, fInitialDownload, fReachedBestHeader);

    if (fReachedBestHeader) {
        // lock main here
        LOCK(cs_main);
        if (infnodeman.updateInfinitynodeList(pindexBestHeader->nHeight)){
            bool updateStm = infnodeman.deterministicRewardStatement(10) &&
                             infnodeman.deterministicRewardStatement(5) &&
                             infnodeman.deterministicRewardStatement(1);
            if (updateStm) infnodeman.calculAllInfinityNodesRankAtLastStm();
        }
        // We must be at the tip already
        fFinished = true;
    }
}
