// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2018 FXTC developers
// Copyright (c) 2018-2019 SIN developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/masternodelist.h>
#include <qt/forms/ui_masternodelist.h>

#include <activemasternode.h>
#include <qt/sinunits.h>
#include <interfaces/wallet.h>
#include <interfaces/node.h>
#include <qt/clientmodel.h>
#include <qt/optionsmodel.h>
#include <qt/guiutil.h>
#include <init.h>
#include <key_io.h>
#include <core_io.h>
#include <masternode-sync.h>
#include <masternodeconfig.h>
#include <masternodeman.h>
//SIN
#include <infinitynode.h>
#include <infinitynodeman.h>
#include <infinitynodemeta.h>
//
#include <sync.h>
#include <wallet/wallet.h>
#include <qt/walletmodel.h>

#include <QDialog>
#include <QInputDialog>
#include <QTimer>
#include <QMessageBox>
#include <QStyleFactory>

// begin nodeSetup
#include <boost/algorithm/string.hpp>
#include "rpc/server.h"
#include "rpc/client.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QSettings>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkReply>

UniValue nodeSetupCallRPC(std::string args);
// end nodeSetup

int GetOffsetFromUtc()
{
#if QT_VERSION < 0x050200
    const QDateTime dateTime1 = QDateTime::currentDateTime();
    const QDateTime dateTime2 = QDateTime(dateTime1.date(), dateTime1.time(), Qt::UTC);
    return dateTime1.secsTo(dateTime2);
#else
    return QDateTime::currentDateTime().offsetFromUtc();
#endif
}

MasternodeList::MasternodeList(const PlatformStyle *platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MasternodeList),
    clientModel(0),
    walletModel(0)
{
    ui->setupUi(this);

    ui->startButton->setEnabled(false);

    int columnAliasWidth = 100;
    int columnAddressWidth = 180;
    int columnProtocolWidth = 80;
    int columnStatusWidth = 80;
    int columnActiveWidth = 130;
    int columnLastSeenWidth = 130;

    ui->tableWidgetMyMasternodes->setColumnWidth(0, columnAliasWidth);
    ui->tableWidgetMyMasternodes->setColumnWidth(1, columnAddressWidth);
    ui->tableWidgetMyMasternodes->setColumnWidth(2, columnProtocolWidth);
    ui->tableWidgetMyMasternodes->setColumnWidth(3, columnStatusWidth);
    ui->tableWidgetMyMasternodes->setColumnWidth(4, columnActiveWidth);
    ui->tableWidgetMyMasternodes->setColumnWidth(5, columnLastSeenWidth);

    ui->tableWidgetMasternodes->setColumnWidth(0, columnAddressWidth);
    ui->tableWidgetMasternodes->setColumnWidth(1, columnProtocolWidth);
    ui->tableWidgetMasternodes->setColumnWidth(2, columnStatusWidth);
    ui->tableWidgetMasternodes->setColumnWidth(3, columnActiveWidth);
    ui->tableWidgetMasternodes->setColumnWidth(4, columnLastSeenWidth);

    ui->dinTable->setColumnWidth(0, 250);
    ui->dinTable->setColumnWidth(1, 60);
    ui->dinTable->setColumnWidth(2, 60);
    ui->dinTable->setColumnWidth(3, 60);
    ui->dinTable->setColumnWidth(4, 250);
    ui->dinTable->setColumnWidth(5, 90);
    ui->dinTable->setColumnWidth(6, 60);
    ui->dinTable->setColumnWidth(7, 250);
    ui->dinTable->setColumnWidth(8, 200);
    ui->dinTable->setColumnWidth(9, 100);

    ui->tableWidgetMyMasternodes->setContextMenuPolicy(Qt::CustomContextMenu);

    QAction *startAliasAction = new QAction(tr("Start alias"), this);
    contextMenu = new QMenu();
    contextMenu->addAction(startAliasAction);
    connect(ui->tableWidgetMyMasternodes, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showContextMenu(const QPoint&)));
    connect(startAliasAction, SIGNAL(triggered()), this, SLOT(on_startButton_clicked()));

    ui->dinTable->setContextMenuPolicy(Qt::CustomContextMenu);

    mCheckNodeAction = new QAction(tr("Check node status"), this);
    contextDINMenu = new QMenu();
    contextDINMenu->addAction(mCheckNodeAction);
    connect(ui->dinTable, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showContextDINMenu(const QPoint&)));
    connect(mCheckNodeAction, SIGNAL(triggered()), this, SLOT(on_checkDINNode()));

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateNodeList()));
    connect(timer, SIGNAL(timeout()), this, SLOT(updateMyNodeList()));
    timer->start(1000);

    fFilterUpdated = false;
    nTimeFilterUpdated = GetTime();
    updateNodeList();
    updateDINList();

    // node setup
    NODESETUP_ENDPOINT_NODE = QString::fromStdString(gArgs.GetArg("-nodesetupurl", "https://setup2dev.sinovate.io/includes/api/nodecp.php"));
    NODESETUP_ENDPOINT_BASIC = QString::fromStdString(gArgs.GetArg("-nodesetupurlbasic", "https://setup2dev.sinovate.io/includes/api/basic.php"));
    NODESETUP_PID = "1";  // "22" for prod
    NODESETUP_CONFIRMS = 2;

    // define timers
    invoiceTimer = new QTimer(this);
    connect(invoiceTimer, SIGNAL(timeout()), this, SLOT(nodeSetupCheckInvoiceStatus()));

    burnPrepareTimer = new QTimer(this);
    connect(burnPrepareTimer, SIGNAL(timeout()), this, SLOT(nodeSetupCheckBurnPrepareConfirmations()));

    burnSendTimer = new QTimer(this);
    connect(burnSendTimer, SIGNAL(timeout()), this, SLOT(nodeSetupCheckBurnSendConfirmations()));

    nodeSetupInitialize();
}

MasternodeList::~MasternodeList()
{
    delete ui;
    delete ConnectionManager;
    delete mCheckNodeAction;
}

void MasternodeList::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model) {
        // try to update list when masternode count changes
        connect(clientModel, SIGNAL(strMasternodesChanged(QString)), this, SLOT(updateNodeList()));
    }
}

void MasternodeList::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
}

void MasternodeList::showContextMenu(const QPoint &point)
{
    QTableWidgetItem *item = ui->tableWidgetMyMasternodes->itemAt(point);
    if(item) contextMenu->exec(QCursor::pos());
}

void MasternodeList::showContextDINMenu(const QPoint &point)
{
    QTableWidgetItem *item = ui->dinTable->itemAt(point);
    if(item)    {
        contextDINMenu->exec(QCursor::pos());
    }
}

void MasternodeList::StartAlias(std::string strAlias)
{
    std::string strStatusHtml;
    strStatusHtml += "<center>Alias: " + strAlias;

    for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
        if(mne.getAlias() == strAlias) {
            std::string strError;
            CMasternodeBroadcast mnb;

            bool fSuccess = CMasternodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), mne.getTxHashBurnFund(), mne.getOutputIndexBurnFund(), strError, mnb);

            if(fSuccess) {
                strStatusHtml += "<br>Successfully started infinitynode.";
                mnodeman.UpdateMasternodeList(mnb, *g_connman);
                mnb.Relay(*g_connman);
                mnodeman.NotifyMasternodeUpdates(*g_connman);
            } else {
                strStatusHtml += "<br>Failed to start infinitynode.<br>Error: " + strError;
            }
            break;
        }
    }
    strStatusHtml += "</center>";

    QMessageBox msg;
    msg.setText(QString::fromStdString(strStatusHtml));
    msg.exec();

    updateMyNodeList(true);
}

void MasternodeList::StartAll(std::string strCommand)
{
    int nCountSuccessful = 0;
    int nCountFailed = 0;
    std::string strFailedHtml;

    for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
        std::string strError;
        CMasternodeBroadcast mnb;

        int32_t nOutputIndex = 0;
        if(!ParseInt32(mne.getOutputIndex(), &nOutputIndex)) {
            continue;
        }

        COutPoint outpoint = COutPoint(uint256S(mne.getTxHash()), nOutputIndex);

        if(strCommand == "start-missing" && mnodeman.Has(outpoint)) continue;

        bool fSuccess = CMasternodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), mne.getTxHashBurnFund(), mne.getOutputIndexBurnFund(), strError, mnb);

        if(fSuccess) {
            nCountSuccessful++;
            mnodeman.UpdateMasternodeList(mnb, *g_connman);
            mnb.Relay(*g_connman);
            mnodeman.NotifyMasternodeUpdates(*g_connman);
        } else {
            nCountFailed++;
            strFailedHtml += "\nFailed to start " + mne.getAlias() + ". Error: " + strError;
        }
    }
    std::vector<std::shared_ptr<CWallet>> wallets = GetWallets();
    CWallet * const pwallet = (wallets.size() > 0) ? wallets[0].get() : nullptr;
    pwallet->Lock();

    std::string returnObj;
    returnObj = strprintf("Successfully started %d infinitynodes, failed to start %d, total %d", nCountSuccessful, nCountFailed, nCountFailed + nCountSuccessful);
    if (nCountFailed > 0) {
        returnObj += strFailedHtml;
    }

    QMessageBox msg;
    msg.setText(QString::fromStdString(returnObj));
    msg.exec();

    updateMyNodeList(true);
}

void MasternodeList::updateMyMasternodeInfo(QString strAlias, QString strAddr, const COutPoint& outpoint)
{
    bool fOldRowFound = false;
    int nNewRow = 0;

    for(int i = 0; i < ui->tableWidgetMyMasternodes->rowCount(); i++) {
        if(ui->tableWidgetMyMasternodes->item(i, 0)->text() == strAlias) {
            fOldRowFound = true;
            nNewRow = i;
            break;
        }
    }

    if(nNewRow == 0 && !fOldRowFound) {
        nNewRow = ui->tableWidgetMyMasternodes->rowCount();
        ui->tableWidgetMyMasternodes->insertRow(nNewRow);
    }

    masternode_info_t infoMn;
    bool fFound = mnodeman.GetMasternodeInfo(outpoint, infoMn);

    QTableWidgetItem *aliasItem = new QTableWidgetItem(strAlias);
    QTableWidgetItem *addrItem = new QTableWidgetItem(fFound ? QString::fromStdString(infoMn.addr.ToString()) : strAddr);
    QTableWidgetItem *protocolItem = new QTableWidgetItem(QString::number(fFound ? infoMn.nProtocolVersion : -1));
    QTableWidgetItem *statusItem = new QTableWidgetItem(QString::fromStdString(fFound ? CMasternode::StateToString(infoMn.nActiveState) : "MISSING"));
    QTableWidgetItem *activeSecondsItem = new QTableWidgetItem(QString::fromStdString(DurationToDHMS(fFound ? (infoMn.nTimeLastPing - infoMn.sigTime) : 0)));
    QTableWidgetItem *lastSeenItem = new QTableWidgetItem(QString::fromStdString(FormatISO8601DateTime(fFound ? infoMn.nTimeLastPing + GetOffsetFromUtc() : 0)));
    QTableWidgetItem *pubkeyItem = new QTableWidgetItem(QString::fromStdString(fFound ? EncodeDestination(infoMn.pubKeyCollateralAddress.GetID()) : ""));

    ui->tableWidgetMyMasternodes->setItem(nNewRow, 0, aliasItem);
    ui->tableWidgetMyMasternodes->setItem(nNewRow, 1, addrItem);
    ui->tableWidgetMyMasternodes->setItem(nNewRow, 2, protocolItem);
    ui->tableWidgetMyMasternodes->setItem(nNewRow, 3, statusItem);
    ui->tableWidgetMyMasternodes->setItem(nNewRow, 4, activeSecondsItem);
    ui->tableWidgetMyMasternodes->setItem(nNewRow, 5, lastSeenItem);
    ui->tableWidgetMyMasternodes->setItem(nNewRow, 6, pubkeyItem);
}

void MasternodeList::updateMyNodeList(bool fForce)
{
    TRY_LOCK(cs_mymnlist, fLockAcquired);
    if(!fLockAcquired) {
        return;
    }
    static int64_t nTimeMyListUpdated = 0;

    // automatically update my masternode list only once in MY_MASTERNODELIST_UPDATE_SECONDS seconds,
    // this update still can be triggered manually at any time via button click
    int64_t nSecondsTillUpdate = nTimeMyListUpdated + MY_MASTERNODELIST_UPDATE_SECONDS - GetTime();
    ui->secondsLabel->setText(QString::number(nSecondsTillUpdate));

    if(nSecondsTillUpdate > 0 && !fForce) return;
    nTimeMyListUpdated = GetTime();

    ui->tableWidgetMasternodes->setSortingEnabled(false);
    for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
        int32_t nOutputIndex = 0;
        if(!ParseInt32(mne.getOutputIndex(), &nOutputIndex)) {
            continue;
        }

        updateMyMasternodeInfo(QString::fromStdString(mne.getAlias()), QString::fromStdString(mne.getIp()), COutPoint(uint256S(mne.getTxHash()), nOutputIndex));
    }
    ui->tableWidgetMasternodes->setSortingEnabled(true);

    // reset "timer"
    ui->secondsLabel->setText("0");
}

void MasternodeList::updateNodeList()
{
    TRY_LOCK(cs_mnlist, fLockAcquired);
    if(!fLockAcquired) {
        return;
    }

    static int64_t nTimeListUpdated = GetTime();

    // to prevent high cpu usage update only once in MASTERNODELIST_UPDATE_SECONDS seconds
    // or MASTERNODELIST_FILTER_COOLDOWN_SECONDS seconds after filter was last changed
    int64_t nSecondsToWait = fFilterUpdated
                            ? nTimeFilterUpdated - GetTime() + MASTERNODELIST_FILTER_COOLDOWN_SECONDS
                            : nTimeListUpdated - GetTime() + MASTERNODELIST_UPDATE_SECONDS;

    if(fFilterUpdated) ui->countLabel->setText(QString::fromStdString(strprintf("Please wait... %d", nSecondsToWait)));
    if(nSecondsToWait > 0) return;

    nTimeListUpdated = GetTime();
    fFilterUpdated = false;

    QString strToFilter;
    ui->countLabel->setText("Updating...");
    ui->tableWidgetMasternodes->setSortingEnabled(false);
    ui->tableWidgetMasternodes->clearContents();
    ui->tableWidgetMasternodes->setRowCount(0);
    std::map<COutPoint, CMasternode> mapMasternodes = mnodeman.GetFullMasternodeMap();
    int offsetFromUtc = GetOffsetFromUtc();

    for(auto& mnpair : mapMasternodes)
    {
        CMasternode mn = mnpair.second;
        // populate list
        // Address, Protocol, Status, Active Seconds, Last Seen, Pub Key
        QTableWidgetItem *addressItem = new QTableWidgetItem(QString::fromStdString(mn.addr.ToString()));
        QTableWidgetItem *protocolItem = new QTableWidgetItem(QString::number(mn.nProtocolVersion));
        QTableWidgetItem *statusItem = new QTableWidgetItem(QString::fromStdString(mn.GetStatus()));
        QTableWidgetItem *activeSecondsItem = new QTableWidgetItem(QString::fromStdString(DurationToDHMS(mn.lastPing.sigTime - mn.sigTime)));
        QTableWidgetItem *lastSeenItem = new QTableWidgetItem(QString::fromStdString(FormatISO8601DateTime(mn.lastPing.sigTime + offsetFromUtc)));
        QTableWidgetItem *pubkeyItem = new QTableWidgetItem(QString::fromStdString(EncodeDestination(mn.pubKeyCollateralAddress.GetID())));

        if (strCurrentFilter != "")
        {
            strToFilter =   addressItem->text() + " " +
                            protocolItem->text() + " " +
                            statusItem->text() + " " +
                            activeSecondsItem->text() + " " +
                            lastSeenItem->text() + " " +
                            pubkeyItem->text();
            if (!strToFilter.contains(strCurrentFilter)) continue;
        }

        ui->tableWidgetMasternodes->insertRow(0);
        ui->tableWidgetMasternodes->setItem(0, 0, addressItem);
        ui->tableWidgetMasternodes->setItem(0, 1, protocolItem);
        ui->tableWidgetMasternodes->setItem(0, 2, statusItem);
        ui->tableWidgetMasternodes->setItem(0, 3, activeSecondsItem);
        ui->tableWidgetMasternodes->setItem(0, 4, lastSeenItem);
        ui->tableWidgetMasternodes->setItem(0, 5, pubkeyItem);
    }

    ui->countLabel->setText(QString::number(ui->tableWidgetMasternodes->rowCount()));
    ui->tableWidgetMasternodes->setSortingEnabled(true);

    updateDINList();
}

void MasternodeList::updateDINList()
{
    if(walletModel && walletModel->getOptionsModel())
    {
        std::map<COutPoint, std::string> mOnchainDataInfo = walletModel->wallet().GetOnchainDataInfo();
        std::map<COutPoint, std::string> mapMynode;
        std::map<std::string, int> mapLockRewardHeight;

        interfaces::Node& node = clientModel->node();
        int nCurrentHeight = node.getNumBlocks();
        ui->currentHeightLabel->setText(QString::number(nCurrentHeight));

        int countNode = 0;
        for(auto &pair : mOnchainDataInfo){
            string s, sDataType = "", sData1 = "", sData2 = "";
            stringstream ss(pair.second);
            int i=0;
            while (getline(ss, s,';')) {
                if (i==0) {
                    sDataType = s;
                }
                if (i==1) {
                    sData1 = s;
                }
                if (i==2) {
                    sData2 = s;
                }
                if(sDataType == "NodeCreation"){
                    mapMynode[pair.first] = sData1;
                } else if(sDataType == "LockReward" && i==2){
                    int nRewardHeight = atoi(sData2);
                    if(nRewardHeight > nCurrentHeight){
                        mapLockRewardHeight[sData1] = nRewardHeight;
                    }
                }
                i++;
            }
        }

        ui->dinTable->setRowCount(mapMynode.size());
        ui->dinTable->setSortingEnabled(true);

        // update used burn tx map
        nodeSetupUsedBurnTxs.clear();

        bool bNeedToQueryAPIServiceId = false;
        int serviceId;
        int k=0;
        for(auto &pair : mapMynode){
            infinitynode_info_t infoInf;
            std::string status = "Unknown", sPeerAddress = "", strIP = "---";
            if(!infnodeman.GetInfinitynodeInfo(pair.first, infoInf)){
                continue;
            }
            CMetadata metadata = infnodemeta.Find(infoInf.metadataID);
            if (metadata.getMetadataHeight() == 0 || metadata.getMetaPublicKey() == "" ){
                status="Incomplete";
            } else {
                status="Ready";

                std::string metaPublicKey = metadata.getMetaPublicKey();
                std::vector<unsigned char> tx_data = DecodeBase64(metaPublicKey.c_str());
                if(tx_data.size() == CPubKey::COMPRESSED_PUBLIC_KEY_SIZE){
                    CPubKey pubKey(tx_data.begin(), tx_data.end());
                    CTxDestination dest = GetDestinationForKey(pubKey, DEFAULT_ADDRESS_TYPE);
                    sPeerAddress = EncodeDestination(dest);
                }
                strIP = metadata.getService().ToString();
                if (sPeerAddress!="")   {
                    int serviceValue = (bDINNodeAPIUpdate) ? -1 : 0;
                    serviceId = nodeSetupGetServiceForNodeAddress( QString::fromStdString(sPeerAddress) );

                    if (serviceId==0 || serviceValue==0)   {   // 0 = not checked
                        bNeedToQueryAPIServiceId = true;
                        nodeSetupSetServiceForNodeAddress( QString::fromStdString(sPeerAddress), serviceValue );  // -1 = reset to checked, not queried
                    }
                }
                // update used burn tx map
                std::string burnfundTxId = infoInf.vinBurnFund.prevout.hash.ToString().substr(0, 16);
LogPrintf("nodeSetupGetUnusedBurnTxs updateDINList %s, %s \n", sPeerAddress, burnfundTxId);
                nodeSetupUsedBurnTxs.insert( { burnfundTxId, 1  } );
            }
            if(infoInf.nExpireHeight < nCurrentHeight){
                status="Expired";
            }
            QString nodeTxId = QString::fromStdString(infoInf.collateralAddress);
            ui->dinTable->setItem(k, 0, new QTableWidgetItem(QString(nodeTxId)));
            ui->dinTable->setItem(k, 1, new QTableWidgetItem(QString::number(infoInf.nHeight)));
            ui->dinTable->setItem(k, 2, new QTableWidgetItem(QString::number(infoInf.nExpireHeight)));
            ui->dinTable->setItem(k, 3, new QTableWidgetItem(QString(QString::fromStdString(status))));
            ui->dinTable->setItem(k, 4, new QTableWidgetItem(QString(QString::fromStdString(strIP))));
            ui->dinTable->setItem(k, 5, new QTableWidgetItem(QString(QString::fromStdString(sPeerAddress))));
            bool flocked = mapLockRewardHeight.find(sPeerAddress) != mapLockRewardHeight.end();
            if(flocked) {
                ui->dinTable->setItem(k, 6, new QTableWidgetItem(QString::number(mapLockRewardHeight[sPeerAddress])));
                ui->dinTable->setItem(k, 7, new QTableWidgetItem(QString(QString::fromStdString("Yes"))));
            } else {
                ui->dinTable->setItem(k, 6, new QTableWidgetItem(QString(QString::fromStdString(""))));
                ui->dinTable->setItem(k, 7, new QTableWidgetItem(QString(QString::fromStdString("No"))));
            }
            ui->dinTable->setItem(k,8, new QTableWidgetItem(QString(QString::fromStdString(pair.second))));
            k++;
        }

        bDINNodeAPIUpdate = true;

        if (bNeedToQueryAPIServiceId)   {
            QString email, pass, strError;
            int clientId = nodeSetupGetClientId( email, pass );
            if (clientId>0) {
                nodeSetupAPINodeList( email, pass, strError );
            }
        }
        // use as nodeSetup combo refresh too
        nodeSetupPopulateInvoicesCombo();
        nodeSetupPopulateBurnTxCombo();
    }
}

void MasternodeList::on_filterLineEdit_textChanged(const QString &strFilterIn)
{
    strCurrentFilter = strFilterIn;
    nTimeFilterUpdated = GetTime();
    fFilterUpdated = true;
    ui->countLabel->setText(QString::fromStdString(strprintf("Please wait... %d", MASTERNODELIST_FILTER_COOLDOWN_SECONDS)));
}

void MasternodeList::on_startButton_clicked()
{
    std::string strAlias;
    {
        LOCK(cs_mymnlist);
        // Find selected node alias
        QItemSelectionModel* selectionModel = ui->tableWidgetMyMasternodes->selectionModel();
        QModelIndexList selected = selectionModel->selectedRows();

        if(selected.count() == 0) return;

        QModelIndex index = selected.at(0);
        int nSelectedRow = index.row();
        strAlias = ui->tableWidgetMyMasternodes->item(nSelectedRow, 0)->text().toStdString();
    }

    // Display message box
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm infinitynode start"),
        tr("Are you sure you want to start infinitynode %1?").arg(QString::fromStdString(strAlias)),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if(retval != QMessageBox::Yes) return;

    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();

    if(encStatus == walletModel->Locked) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock());

        if(!ctx.isValid()) return; // Unlock wallet was cancelled

        StartAlias(strAlias);
        return;
    }

    StartAlias(strAlias);
}

void MasternodeList::on_startAllButton_clicked()
{
    // Display message box
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm all Infinitynodes start"),
        tr("Are you sure you want to start ALL InfinityNodes?"),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if(retval != QMessageBox::Yes) return;

    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();

    if(encStatus == walletModel->Locked) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock());

        if(!ctx.isValid()) return; // Unlock wallet was cancelled

        StartAll();
        return;
    }

    StartAll();
}

void MasternodeList::on_tableWidgetMyMasternodes_itemSelectionChanged()
{
    if(ui->tableWidgetMyMasternodes->selectedItems().count() > 0) {
        ui->startButton->setEnabled(true);
    }
}

void MasternodeList::on_UpdateButton_clicked()
{
    updateMyNodeList(true);
}

void MasternodeList::on_checkDINNode()
{
    QItemSelectionModel* selectionModel = ui->dinTable->selectionModel();
    QModelIndexList selected = selectionModel->selectedRows();
    if(selected.count() == 0) return;

    QModelIndex index = selected.at(0);
    int nSelectedRow = index.row();
    QString strAddress = ui->dinTable->item(nSelectedRow, 5)->text();
    QString strError;
    QString strStatus = ui->dinTable->item(nSelectedRow, 3)->text();
    QMessageBox msg;

    if ( strStatus!="Ready")    {
        msg.setText(tr("DIN node must be in Ready status"));
        msg.exec();
    }
    else    {
        mCheckNodeAction->setEnabled(false);
        int serviceId = nodeSetupGetServiceForNodeAddress( strAddress );
        if (serviceId > 0)    {
            QString email, pass, strError;
            int clientId = nodeSetupGetClientId( email, pass );
            if (clientId>0) {
                QJsonObject obj = nodeSetupAPINodeInfo( serviceId, mClientid , email, pass, strError );
                if (obj.contains("Blockcount") && obj.contains("MyPeerInfo"))   {
                    int blockCount = obj["Blockcount"].toInt();
                    QString peerInfo = obj["MyPeerInfo"].toString();
                    ui->dinTable->setItem(nSelectedRow, 9, new QTableWidgetItem(QString::number(blockCount)));
                    ui->dinTable->setItem(nSelectedRow, 10, new QTableWidgetItem(peerInfo));
                }
            }
            else    {
                msg.setText("Could not recover node's client ID\nPlease log in with your user email and password in the Node Setup tab");
                msg.exec();
            }
        }
        else    {
            msg.setText("Could not recover node's service ID\nPlease log in with your user email and password in the Node Setup tab");
            msg.exec();
        }
        mCheckNodeAction->setEnabled(true);
    }
}

// nodeSetup buttons
void MasternodeList::on_btnCheck_clicked()
{
    nodeSetupCleanProgress();
    if ( !nodeSetupCheckFunds() )   {
        ui->labelMessage->setText("You didn't pass the checks. Please review.");
        return;
    }

    // TODO continue
    ui->labelMessage->setText("You passed all checks. Please select a billing period and press Place Order to continue.");
    ui->btnSetup->setEnabled(true);
    return;
}

void MasternodeList::on_btnSetup_clicked()
{
    // check again in case they changed the tier...
    nodeSetupCleanProgress();
    if ( !nodeSetupCheckFunds() )   {
        ui->labelMessage->setText("You didn't pass the funds check. Please review.");
        return;
    }

    int orderid, invoiceid, productid;
    QString strError;
    QString strBillingCycle = QString::fromStdString(billingOptions[ui->comboBilling->currentData().toInt()]);

//LogPrintf("place order %d, %s ", mClientid, strBillingCycle);
    if ( ! (mOrderid > 0 && mInvoiceid > 0) ) {     // place new order if there is none already
        mOrderid = nodeSetupAPIAddOrder( mClientid, strBillingCycle, mProductIds, mInvoiceid, strError );
    }

    if ( mOrderid > 0 && mInvoiceid > 0) {
        nodeSetupSetOrderId( mOrderid, mInvoiceid, mProductIds );
        nodeSetupEnableOrderUI(true, mOrderid, mInvoiceid);
        ui->labelMessage->setText(QString::fromStdString(strprintf("Order placed successfully. Order ID #%d Invoice ID #%d", mOrderid, mInvoiceid)));

        // get invoice data and do payment
        QString strAmount, strStatus, paymentAddress;
        strStatus = nodeSetupCheckInvoiceStatus();
    }
    else    {
        ui->labelMessage->setText(strError);
    }
}

void MasternodeList::on_payButton_clicked()
{
     int invoiceToPay = ui->comboInvoice->currentData().toInt();
     QString strAmount, strStatus, paymentAddress, strError;

     if (invoiceToPay>0)    {
        nodeSetupAPIGetInvoice( invoiceToPay, strAmount, strStatus, paymentAddress, strError );
        CAmount invoiceAmount = strAmount.toDouble();
         if ( strStatus == "Unpaid" )  {
             QString paymentTx = nodeSetupSendToAddress( paymentAddress, invoiceAmount, NULL );
             if ( paymentTx != "" ) {
                 ui->labelMessage->setText( "Pending Invoice Payment finished, please wait for confirmations." );
             }
         }
     }
}

QString MasternodeList::nodeSetupGetNewAddress()    {

    QString strAddress = "";
    std::ostringstream cmd;
    try {
        cmd.str("");
        cmd << "getnewaddress";
        UniValue jsonVal = nodeSetupCallRPC( cmd.str() );
        if ( jsonVal.type() == UniValue::VSTR )       // new address returned
        {
            strAddress = QString::fromStdString(jsonVal.get_str());
        }
    } catch (UniValue& objError ) {
        ui->labelMessage->setText( "Error getting new wallet address" );
    }
    return strAddress;
}

QString MasternodeList::nodeSetupSendToAddress( QString strAddress, int amount, QTimer* timer )    {
    QString strTxId = "";
    std::ostringstream cmd;
    try {
        cmd.str("");
        cmd << "sendtoaddress " << strAddress.toUtf8().constData() << " " << amount;
        UniValue jsonVal = nodeSetupCallRPC( cmd.str() );
        if ( jsonVal.type() == UniValue::VSTR )       // tx id returned
        {
            strTxId = QString::fromStdString(jsonVal.get_str());
            if ( timer!=NULL && !timer->isActive() )  {
                timer->start(20000);
            }
        }
    } catch (UniValue& objError ) {
        QString errMessage = QString::fromStdString(find_value(objError, "message").get_str());
        if ( errMessage.contains("walletpassphrase") )  {
            QMessageBox::warning(this, "Please Unlock Wallet", "In order to make payments, please unlock your wallet and retry", QMessageBox::Ok, QMessageBox::Ok);
        }

        ui->labelMessage->setText( QString::fromStdString(find_value(objError, "message").get_str()) );
    }
    return strTxId;
}

UniValue MasternodeList::nodeSetupGetTxInfo( QString txHash, std::string attribute)  {
    UniValue ret;
    std::ostringstream cmd;
    try {
        cmd.str("");
        cmd << "gettransaction " << txHash.toUtf8().constData();
        UniValue jsonVal = nodeSetupCallRPC( cmd.str() );
        if ( jsonVal.type() == UniValue::VOBJ )       // object returned
        {
            ret = find_value(jsonVal.get_obj(), attribute);
        }
        else {
            ui->labelMessage->setText( "Error calling RPC gettransaction");
        }
    } catch (UniValue& objError ) {
        ui->labelMessage->setText( "Error calling RPC gettransaction");
    }
    return ret;
}

QString MasternodeList::nodeSetupCheckInvoiceStatus()  {
    QString strAmount, strStatus, paymentAddress, strError;
    nodeSetupAPIGetInvoice( mInvoiceid, strAmount, strStatus, paymentAddress, strError );

    CAmount invoiceAmount = strAmount.toDouble();
ui->labelMessage->setText(QString::fromStdString(strprintf("Invoice amount %f SIN", invoiceAmount)));
//LogPrintf("nodeSetupCheckInvoiceStatus %s, %s, %f\n", strStatus.toStdString(), paymentAddress.toStdString(), invoiceAmount );
    if ( strStatus == "Cancelled" || strStatus == "Refunded" )  {  // reset and call again
        nodeSetupStep( "setupWait", "Order cancelled or refunded, creating a new order");
        invoiceTimer->stop();
        nodeSetupResetOrderId();
        on_btnSetup_clicked();
    }

    if ( strStatus == "Unpaid" )  {
        if ( mPaymentTx != "" ) {   // already paid, waiting confirmations
LogPrintf("nodeSetupCheckInvoiceStatus %s \n", mPaymentTx.toStdString() );
            nodeSetupStep( "setupWait", "Invoice paid, waiting for confirmation");
            ui->btnSetup->setEnabled(false);
            ui->btnSetupReset->setEnabled(false);
        }
        else    {
LogPrintf("nodeSetupCheckInvoiceStatus no txID \n");
            nodeSetupStep( "setupWait", "Paying invoice");
            // Display message box
            QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm Invoice Payment"),
                "Are you sure you want to pay " + QString::number(invoiceAmount) + " SIN?",
                QMessageBox::Yes | QMessageBox::Cancel,
                QMessageBox::Cancel);

            if(retval != QMessageBox::Yes)  {
                invoiceTimer->stop();
                ui->btnSetup->setEnabled(true);
                ui->btnSetupReset->setEnabled(true);
                ui->labelMessage->setText( "Press Reset Order button to cancel node setup process, or Continue Order button to resume." );
                return "cancelled";
            }

            mPaymentTx = nodeSetupSendToAddress( paymentAddress, invoiceAmount, invoiceTimer );
            if ( mPaymentTx != "" ) {
                nodeSetupSetPaymentTx(mPaymentTx);
                ui->labelMessage->setText( "Payment finished, please wait until platform confirms payment to proceed to node creation." );
                ui->btnSetup->setEnabled(false);
                ui->btnSetupReset->setEnabled(false);
                if ( !invoiceTimer->isActive() )  {
                    invoiceTimer->start(60000);
                }
            }
        }
    }

    if ( strStatus == "Paid" )  {           // launch node setup (RPC)
LogPrintf("nodeSetupCheckInvoiceStatus Invoice Paid \n");
        invoiceTimer->stop();

        QString strPrivateKey, strPublicKey, strDecodePublicKey, strAddress, strNodeIp;

        mBurnTx = nodeSetupGetBurnTx();
        QString strSelectedBurnTx = ui->comboBurnTx->currentData().toString();

        if ( mBurnTx=="" && strSelectedBurnTx!="NEW")   mBurnTx = strSelectedBurnTx;

        if ( mBurnTx!="" )   {   // skip to check burn tx
            if ( !burnSendTimer->isActive() )  {
                burnSendTimer->start(20000);    // check every 20 secs
            }
        }
        else    {   // burn tx not made yet
            mBurnAddress = nodeSetupGetNewAddress();
            int nMasternodeBurn = nodeSetupGetBurnAmount();

            mBurnPrepareTx = nodeSetupSendToAddress( mBurnAddress, nMasternodeBurn, burnPrepareTimer );
    LogPrintf("nodeSetupSendToAddress %s \n", mBurnPrepareTx.toStdString());
            if ( mBurnPrepareTx=="" )  {
               ui->labelMessage->setText( "ERROR: failed to prepare burn transaction." );
            }
        }
        nodeSetupStep( "setupWait", "Preparing burn transaction");
    }

    return strStatus;
}

void MasternodeList::nodeSetupCheckBurnPrepareConfirmations()   {

    UniValue objConfirms = nodeSetupGetTxInfo( mBurnPrepareTx, "confirmations" );
    int numConfirms = objConfirms.get_int();
    if ( numConfirms>NODESETUP_CONFIRMS )    {
        nodeSetupStep( "setupOk", "Sending burn transaction");
        burnPrepareTimer->stop();
        QString strAddressBackup = nodeSetupGetNewAddress();
        int nMasternodeBurn = nodeSetupGetBurnAmount();

        mBurnTx = nodeSetupRPCBurnFund( mBurnAddress, nMasternodeBurn , strAddressBackup);
LogPrintf("nodeSetupCheckBurnPrepareConfirmations burntx=%s \n", mBurnTx.toStdString());
        QString metaTx = nodeSetupSendToAddress( mBurnAddress, 5 , NULL );
        if ( mBurnTx!="" )  {
            nodeSetupSetBurnTx(mBurnTx);
            if ( !burnSendTimer->isActive() )  {
                burnSendTimer->start(20000);    // check every 20 secs
            }
        }
        else    {
            ui->labelMessage->setText( "ERROR: failed to create burn transaction." );
        }
    }
}

void MasternodeList::nodeSetupCheckBurnSendConfirmations()   {

    // recover data
    QString email, pass, strError;
    int clientId = nodeSetupGetClientId( email, pass );

    UniValue objConfirms = nodeSetupGetTxInfo( mBurnTx, "confirmations" );
    int numConfirms = objConfirms.get_int();
    if ( numConfirms>NODESETUP_CONFIRMS )    {
        nodeSetupStep( "setupKo", "Finishing node setup");
        burnSendTimer->stop();

        QJsonObject root = nodeSetupAPIInfo( mServiceId, clientId, email, pass, strError );
LogPrintf("nodeSetupCheckBurnSendConfirmations %d, %d, %s, %s \n", mServiceId, clientId, email.toStdString(), pass.toStdString());
        if ( root.contains("PrivateKey") ) {
            QString strPrivateKey = root["PrivateKey"].toString();
            QString strPublicKey = root["PublicKey"].toString();
            QString strDecodePublicKey = root["DecodePublicKey"].toString();
            QString strAddress = root["Address"].toString();
            QString strNodeIp = root["Nodeip"].toString();

            std::ostringstream cmd;

            try {
                cmd.str("");
                cmd << "infinitynodeupdatemeta " << mBurnAddress.toUtf8().constData() << " " << strPublicKey.toUtf8().constData() << " " << strNodeIp.toUtf8().constData() << " " << mBurnTx.left(16).toUtf8().constData();
                UniValue jsonVal = nodeSetupCallRPC( cmd.str() );

                nodeSetupSetServiceForNodeAddress( strAddress, mServiceId); // store serviceid
                // cleanup
                nodeSetupResetOrderId();
                nodeSetupSetBurnTx("");

                nodeSetupStep( "setupKo", "Node setup finished");
            }
            catch (const UniValue& objError)
            {
                ui->labelMessage->setText( QString::fromStdString(find_value(objError, "message").get_str()) );
            }
            catch ( std::runtime_error e)
            {
                ui->labelMessage->setText( QString::fromStdString( "ERROR infinitynodeupdatemeta: unexpected error " ) + QString::fromStdString( e.what() ));
            }
        }
        else    {
            LogPrintf("infinitynodeupdatemeta Error while obtaining node info \n");
            ui->labelMessage->setText( "ERROR: infinitynodeupdatemeta " );
        }
    }
}

/*
 * To setup DIN node, we do by RPC command. We need Qt to replace RPC command
RPC steps :
infinitynodeburnfund your_Burn_Address Amount Your_Backup_Address
cBurnAddress
    Sample for MINI
    infinitynodeburnfund SQ5Qnpf3mWituuXtQrEknKYDaKUtinvMzT 100000 SWyHHvnPNaH18TcfszAzWfKyojmTqtZQqy
 *
 */

QString MasternodeList::nodeSetupRPCBurnFund( QString collateralAddress, CAmount amount, QString backupAddress ) {
    QString burnTx = "";
    std::ostringstream cmd;

    try {
        cmd.str("");
        cmd << "infinitynodeburnfund " << collateralAddress.toUtf8().constData() << " " << amount << " " << backupAddress.toUtf8().constData();
        UniValue jsonVal = nodeSetupCallRPC( cmd.str() );
        if ( jsonVal.isStr() )       // some error happened, cannot continue
        {
            ui->labelMessage->setText(QString::fromStdString( "ERROR infinitynodeburnfund: " ) + QString::fromStdString(jsonVal.get_str()));
        }
        else if ( jsonVal.isArray() ){
            UniValue jsonArr = jsonVal.get_array();
            if (jsonArr.size()>0)   {
                UniValue jsonObj = jsonArr[0].get_obj();
                burnTx = QString::fromStdString(find_value(jsonObj, "BURNTX").get_str());
            }
        }
        else {
            ui->labelMessage->setText(QString::fromStdString( "ERROR infinitynodeburnfund: unknown response") );
        }
    }
    catch (const UniValue& objError)
    {
        ui->labelMessage->setText( QString::fromStdString(find_value(objError, "message").get_str()) );
    }
    catch ( std::runtime_error e)
    {
        ui->labelMessage->setText( QString::fromStdString( "ERROR infinitynodeburnfund: unexpected error " ) + QString::fromStdString( e.what() ));
    }
    return burnTx;
}

void MasternodeList::on_btnLogin_clicked()
{
    QString strError = "";
LogPrintf("nodeSetup login \n");
    if ( mClientid > 0 )    {   // reset
        nodeSetupResetClientId();
        return;
    }

    int clientId = nodeSetupAPIAddClient( ui->txtFirstName->text(), ui->txtLastName->text(), ui->txtEmail->text(), ui->txtPassword->text(), strError );
    if ( strError != "" )  {
        ui->labelMessage->setText( strError );
    }

    if ( clientId > 0 ) {
        nodeSetupEnableClientId( clientId );
        nodeSetupSetClientId( clientId, ui->txtEmail->text(), ui->txtPassword->text() );
LogPrintf("nodeSetup enable %d\n", clientId);
    }
}

void MasternodeList::on_btnSetupReset_clicked()
{
    nodeSetupSetOrderId(0, 0, "");
    nodeSetupEnableOrderUI(false);
}

// END nodeSetup buttons

void MasternodeList::nodeSetupInitialize()   {
    ConnectionManager = new QNetworkAccessManager(this);

    labelPic[0] = ui->labelPic_1;
    labelTxt[0] = ui->labelTxt_1;
    labelPic[1] = ui->labelPic_2;
    labelTxt[1] = ui->labelTxt_2;
    labelPic[2] = ui->labelPic_3;
    labelTxt[2] = ui->labelTxt_3;
    labelPic[3] = ui->labelPic_4;
    labelTxt[3] = ui->labelTxt_4;
    labelPic[4] = ui->labelPic_5;
    labelTxt[4] = ui->labelTxt_5;
    labelPic[5] = ui->labelPic_6;
    labelTxt[5] = ui->labelTxt_6;
    labelPic[6] = ui->labelPic_7;
    labelTxt[6] = ui->labelTxt_7;
    labelPic[7] = ui->labelPic_8;
    labelTxt[7] = ui->labelTxt_8;

    // combo billing
    for (int i=0; i<sizeof(billingOptions)/sizeof(billingOptions[0]); i++)    {
        std::string option = billingOptions[i];
        ui->comboBilling->addItem(QString::fromStdString(option), QVariant(i));
    }

#if defined(Q_OS_WIN)
#else
    ui->comboBilling->setStyle(QStyleFactory::create("Windows"));
    ui->comboInvoice->setStyle(QStyleFactory::create("Windows"));
    ui->comboBurnTx->setStyle(QStyleFactory::create("Windows"));
#endif

    // buttons
    ui->btnSetup->setEnabled(false);

    // progress lines
    nodeSetupCleanProgress();

    // recover data
    QString email, pass;

    int clientId = nodeSetupGetClientId( email, pass );
    if ( clientId == 0 )    {
        ui->widgetLogin->show();
        ui->widgetCurrent->hide();
        ui->labelClientId->setText("");
    }
    else {
        nodeSetupEnableClientId(clientId);
    }

    mOrderid = nodeSetupGetOrderId( mInvoiceid, mProductIds );
    if ( mOrderid > 0 )    {
        ui->labelMessage->setText(QString::fromStdString(strprintf("There is an order ongoing (#%d). Press 'Continue' or 'Reset' order.", mOrderid)));
        nodeSetupEnableOrderUI(true, mOrderid, mInvoiceid);
        mPaymentTx = nodeSetupGetPaymentTx();
    }
    else    {
        nodeSetupEnableOrderUI(false);
    }

}

void MasternodeList::nodeSetupEnableOrderUI( bool bEnable, int orderID , int invoiceID ) {
    if (bEnable)    {
        ui->btnCheck->setEnabled(false);
        ui->btnSetup->setEnabled(true);
        ui->btnSetupReset->setEnabled(true);
        ui->labelOrder->setVisible(true);
        ui->labelOrderID->setVisible(true);
        ui->labelOrderID->setText(QString::fromStdString("#")+QString::number(orderID));
        ui->btnSetup->setText(QString::fromStdString("Continue Order"));
        ui->labelInvoice->setVisible(true);
        ui->labelInvoiceID->setVisible(true);
        ui->labelInvoiceID->setText(QString::fromStdString("#")+QString::number(mInvoiceid));
    }
    else {
        ui->btnCheck->setEnabled(true);
        ui->btnSetup->setEnabled(false);
        ui->btnSetupReset->setEnabled(false);
        ui->labelOrder->setVisible(false);
        ui->labelOrderID->setVisible(false);
        ui->labelInvoice->setVisible(false);
        ui->labelInvoiceID->setVisible(false);
    }
}

void MasternodeList::nodeSetupResetClientId( )  {
    nodeSetupSetClientId( 0 , "", "");
    ui->widgetLogin->show();
    ui->widgetCurrent->hide();
    ui->labelClientId->setText("");
    ui->btnLogin->setText("Create/Login");
    ui->btnCheck->setEnabled(false);
    ui->btnSetup->setEnabled(false);
    ui->btnCheck->setEnabled(false);
    mClientid = 0;
    nodeSetupResetOrderId();
    ui->labelMessage->setText("Enter your client data and create a new user or login an existing one.");
}

void MasternodeList::nodeSetupResetOrderId( )   {
    nodeSetupSetOrderId( 0, 0, "");
    ui->btnSetupReset->setEnabled(false);
    ui->btnSetup->setEnabled(false);
    ui->btnSetup->setText(QString::fromStdString("Place Order"));
    ui->btnCheck->setEnabled(true);
    ui->labelMessage->setText("Select a node Tier and then press 'Check' to verify if you meet the prerequisites");
    mOrderid = mInvoiceid = mServiceId = 0;
    mPaymentTx = "";
    nodeSetupSetBurnTx("");
}

void MasternodeList::nodeSetupEnableClientId( int clientId )  {
    ui->widgetLogin->hide();
    ui->widgetCurrent->show();
    ui->labelClientId->setText("#"+QString::number(clientId));
    ui->btnCheck->setEnabled(true);
    ui->labelMessage->setText("Select a node Tier and then press 'Check' to verify if you meet the prerequisites");
    mClientid = clientId;
    ui->btnLogin->setText("Logout");

    nodeSetupPopulateInvoicesCombo();
    ui->comboBurnTx->addItem(tr("Loading node data..."),"WAIT");
}

void MasternodeList::nodeSetupPopulateInvoicesCombo( )  {
    QString email, pass, strError;
    int clientId = nodeSetupGetClientId( email, pass );
    std::map<int, std::string> pendingInvoices = nodeSetupAPIListInvoices( email, pass, strError );

    // preserve previous selection before clearing
    int invoiceToPay = ui->comboInvoice->currentData().toInt();
    ui->comboInvoice->clear();

    // populate
    for(auto& itemPair : pendingInvoices)   {
        if (mInvoiceid!=itemPair.first) {       // discard current setup invoice from pending invoices combo
            ui->comboInvoice->addItem(QString::fromStdString(itemPair.second), QVariant(itemPair.first));
        }
    }

    // restore selection (if still exists)
    int index = ui->comboInvoice->findData(invoiceToPay);
    if ( index != -1 ) { // -1 for not found
       ui->comboInvoice->setCurrentIndex(index);
    }
}

void MasternodeList::nodeSetupPopulateBurnTxCombo( )  {
    std::map<std::string, std::string> freeBurnTxs = nodeSetupGetUnusedBurnTxs( );

    // preserve previous selection before clearing
    QString burnTxSelection = ui->comboBurnTx->currentData().toString();

    ui->comboBurnTx->clear();
    ui->comboBurnTx->addItem(tr("<Create new>"),"NEW");

    for(auto& itemPair : freeBurnTxs)   {
        ui->comboBurnTx->addItem(QString::fromStdString(itemPair.second), QVariant(QString::fromStdString(itemPair.first)));
    }

    // restore selection (if still exists)
    int index = ui->comboBurnTx->findData(burnTxSelection);
    if ( index != -1 ) { // -1 for not found
       ui->comboBurnTx->setCurrentIndex(index);
    }

}

int MasternodeList::nodeSetupGetBurnAmount()    {
    int nMasternodeBurn = 0;

    if ( ui->radioLILNode->isChecked() )    nMasternodeBurn = Params().GetConsensus().nMasternodeBurnSINNODE_1;
    if ( ui->radioMIDNode->isChecked() )    nMasternodeBurn = Params().GetConsensus().nMasternodeBurnSINNODE_5;
    if ( ui->radioBIGNode->isChecked() )    nMasternodeBurn = Params().GetConsensus().nMasternodeBurnSINNODE_10;

    return nMasternodeBurn;
}

bool MasternodeList::nodeSetupCheckFunds( CAmount invoiceAmount )   {

    bool bRet = false;
    int nMasternodeCollateral = Params().GetConsensus().nMasternodeCollateralMinimum;
    int nMasternodeBurn = nodeSetupGetBurnAmount();

    std::string strChecking = "Checking funds";
    nodeSetupStep( "setupWait", strChecking );

    std::vector<std::shared_ptr<CWallet>> wallets = GetWallets();
    CWallet * const pwallet = (wallets.size() > 0) ? wallets[0].get() : nullptr;
    CAmount curBalance = pwallet->GetBalance();
    std::ostringstream stringStream;
    CAmount nNodeRequirement = (nMasternodeBurn + nMasternodeCollateral) * COIN ;

    if ( curBalance > invoiceAmount + nNodeRequirement )  {
        nodeSetupStep( "setupOk", strChecking + " : " + "funds available.");
        bRet = true;
    }
    else    {
        if ( curBalance > nNodeRequirement )  {
            QString strAvailable = BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), curBalance - nNodeRequirement );
            QString strInvoiceAmount = BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), invoiceAmount );
            stringStream << strChecking << " : not enough funds to pay invoice. (you have " << strAvailable.toStdString() << " , need " << strInvoiceAmount.toStdString() << " )";
            std::string copyOfStr = stringStream.str();
                nodeSetupStep( "setupKo", copyOfStr);
        }
        else if ( curBalance > nMasternodeBurn * COIN )  {
            QString strAvailable = BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), (curBalance-(nMasternodeBurn*COIN)) );
            QString strCollateral = BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), nMasternodeCollateral*COIN );
            stringStream << strChecking << " : not enough collateral (you have " <<  strAvailable.toStdString() << " , you need " << strCollateral.toStdString() << " )";
            std::string copyOfStr = stringStream.str();
            nodeSetupStep( "setupKo", copyOfStr);
        }
        else if ( curBalance > nNodeRequirement )  {
            QString strAvailable = BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), curBalance );
            QString strBurnAmount = BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), nMasternodeBurn*COIN );
            stringStream << strChecking << " : not enough funds to burn. (you have " << strAvailable.toStdString() << " , need " << strBurnAmount.toStdString() << " )";
            std::string copyOfStr = stringStream.str();
            nodeSetupStep( "setupKo", copyOfStr);
        }
        else    {

        }
    }

    currentStep++;
    return bRet;
}

int MasternodeList::nodeSetupGetClientId( QString& email, QString& pass )  {
    int ret = 0;
    QSettings settings;

    if (settings.contains("nodeSetupClientId"))
        ret = settings.value("nodeSetupClientId").toInt();

    if (settings.contains("nodeSetupEmail"))
        email = settings.value("nodeSetupEmail").toString();

    if (settings.contains("nodeSetupPassword"))
        pass = settings.value("nodeSetupPassword").toString();

    return ret;
}

void MasternodeList::nodeSetupSetClientId( int clientId, QString email, QString pass )  {
    QSettings settings;
    settings.setValue("nodeSetupClientId", clientId);
    settings.setValue("nodeSetupEmail", email);
    settings.setValue("nodeSetupPassword", pass);
}

int MasternodeList::nodeSetupGetOrderId( int& invoiceid, QString& productids )  {
    int ret = 0;
    QSettings settings;

    if (settings.contains("nodeSetupOrderId"))
        ret = settings.value("nodeSetupOrderId").toInt();

    if (settings.contains("nodeSetupInvoiceId"))
        invoiceid = settings.value("nodeSetupInvoiceId").toInt();

    if (settings.contains("nodeSetupProductIds"))
        productids = settings.value("nodeSetupProductIds").toString();

    return ret;
}

void MasternodeList::nodeSetupSetOrderId( int orderid , int invoiceid, QString productids )  {
    QSettings settings;
    settings.setValue("nodeSetupOrderId", orderid);
    settings.setValue("nodeSetupInvoiceId", invoiceid);
    settings.setValue("nodeSetupProductIds", productids);
}

QString MasternodeList::nodeSetupGetBurnTx( )  {
    QString ret = 0;
    QSettings settings;

    if (settings.contains("nodeSetupBurnTx"))
        ret = settings.value("nodeSetupBurnTx").toString();

    return ret;
}

void MasternodeList::nodeSetupSetBurnTx( QString strBurnTx )  {
    QSettings settings;
    settings.setValue("nodeSetupBurnTx", strBurnTx);
}

int MasternodeList::nodeSetupGetServiceForNodeAddress( QString nodeAdress ) {
    int ret = 0;
    QSettings settings;
    QString key = "nodeSetupService"+nodeAdress;

    if (settings.contains(key))
        ret = settings.value(key).toInt();

    return ret;
}

void MasternodeList::nodeSetupSetServiceForNodeAddress( QString nodeAdress, int serviceId )  {
    QSettings settings;
    QString key = "nodeSetupService"+nodeAdress;

    settings.setValue(key, serviceId);
}

QString MasternodeList::nodeSetupGetPaymentTx( )  {
    QString ret = 0;
    QSettings settings;

    if (settings.contains("nodeSetupPaymentTx"))
        ret = settings.value("nodeSetupPaymentTx").toString();

    return ret;
}

void MasternodeList::nodeSetupSetPaymentTx( QString txHash )  {
    QSettings settings;
    settings.setValue("nodeSetupPaymentTx", txHash);
}

int MasternodeList::nodeSetupAPIAddClient( QString firstName, QString lastName, QString email, QString password, QString& strError )  {
    int ret = 0;

    QString Service = QString::fromStdString("AddClient");
    QUrl url( MasternodeList::NODESETUP_ENDPOINT_BASIC );
    QUrlQuery urlQuery( url );
    urlQuery.addQueryItem("action", Service);
    urlQuery.addQueryItem("firstname", firstName);
    urlQuery.addQueryItem("lastname", lastName);
    urlQuery.addQueryItem("email", email);
    urlQuery.addQueryItem("password2", password);
    url.setQuery( urlQuery );

    QNetworkRequest request( url );

LogPrintf("nodeSetup::AddClient -- %s\n", url.toString().toStdString());
    QNetworkReply *reply = ConnectionManager->get(request);
    QEventLoop loop;

    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();

    QByteArray data = reply->readAll();
    QJsonDocument json = QJsonDocument::fromJson(data);

    if ( json.object().contains("clientid") )   {
        ret = json.object()["clientid"].toInt();
    }

    if ( json.object().contains("result") && json.object()["result"]=="error" && ret == 0 && json.object().contains("message")) {
        strError = json.object()["message"].toString();
    }
    return ret;
}

int MasternodeList::nodeSetupAPIAddOrder( int clientid, QString billingCycle, QString& productids, int& invoiceid, QString& strError )  {
    int orderid = 0;

    QString Service = QString::fromStdString("AddOrder");
    QUrl url( MasternodeList::NODESETUP_ENDPOINT_BASIC );
    QUrlQuery urlQuery( url );
    urlQuery.addQueryItem("action", Service);
    urlQuery.addQueryItem("clientid", QString::number(clientid));
    urlQuery.addQueryItem("pid", NODESETUP_PID );
    urlQuery.addQueryItem("domain", "nodeSetup.sinovate.io");
    urlQuery.addQueryItem("billingcycle", billingCycle);
    urlQuery.addQueryItem("paymentmethod", "sin");
    url.setQuery( urlQuery );

    QNetworkRequest request( url );
LogPrintf("nodeSetup::AddOrder -- %s\n", url.toString().toStdString());
    QNetworkReply *reply = ConnectionManager->get(request);
    QEventLoop loop;

    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();

    QByteArray data = reply->readAll();
    QJsonDocument json = QJsonDocument::fromJson(data);

    if ( json.object().contains("result") ) {
        if ( json.object()["result"]=="success" ) {
            if ( json.object().contains("orderid") ) {
                orderid = json.object()["orderid"].toInt();
            }
            if ( json.object().contains("productids") ) {
                productids = json.object()["productids"].toString();
            }
            if ( json.object().contains("invoiceid") ) {
                invoiceid = json.object()["invoiceid"].toInt();
            }
        }
        else    {
            if ( json.object().contains("message") )    {
                strError = json.object()["message"].toString();
            }
        }
    }

    return orderid;
}

bool MasternodeList::nodeSetupAPIGetInvoice( int invoiceid, QString& strAmount, QString& strStatus, QString& paymentAddress, QString& strError )  {
    bool ret = false;

    QString Service = QString::fromStdString("GetInvoice");
    QUrl url( MasternodeList::NODESETUP_ENDPOINT_BASIC );
    QUrlQuery urlQuery( url );
    urlQuery.addQueryItem("action", Service);
    urlQuery.addQueryItem("invoiceid", QString::number(invoiceid));
    url.setQuery( urlQuery );

    QNetworkRequest request( url );
LogPrintf("nodeSetup::GetInvoice -- %s\n", url.toString().toStdString());
    QNetworkReply *reply = ConnectionManager->get(request);
    QEventLoop loop;

    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();

    QByteArray data = reply->readAll();
    QJsonDocument json = QJsonDocument::fromJson(data);
    QJsonObject root = json.object();

    if ( root.contains("result") ) {
        if ( root["result"]=="success" && root.contains("transactions") ) {
// LogPrintf("nodeSetup::GetInvoice result \n" );
            if ( root.contains("status") ) {
                strStatus = root["status"].toString();
                LogPrintf("nodeSetup::GetInvoice contains status %s \n", strStatus.toStdString() );
            }

            QJsonArray jsonArray = root["transactions"].toObject()["transaction"].toArray();
            QJsonObject tx = jsonArray.first().toObject();
            if ( tx.contains("description") ) {
                strAmount = tx["description"].toString();
            }

            if ( tx.contains("transid") ) {
                paymentAddress = tx["transid"].toString();
            }

            QJsonArray jsonArray2 = root["items"].toObject()["item"].toArray();
            QJsonObject item = jsonArray2.first().toObject();
            if ( item.contains("relid") ) {
                mServiceId = item["relid"].toInt();
            }

            ret = true;
        }
        else    {
            if ( root.contains("message") )    {
                strError = root["message"].toString();
            }
        }
    }

    return ret;
}

std::map<int,std::string> MasternodeList::nodeSetupAPIListInvoices( QString email, QString password, QString& strError )    {
    std::map<int,std::string> ret;

    QString Service = QString::fromStdString("ListInvoices");
    QUrl url( MasternodeList::NODESETUP_ENDPOINT_BASIC );
    QUrlQuery urlQuery( url );
    urlQuery.addQueryItem("action", Service);
    urlQuery.addQueryItem("email", email);
    urlQuery.addQueryItem("password2", password);
    url.setQuery( urlQuery );

    QNetworkRequest request( url );
LogPrintf("nodeSetup::ListInvoices %s \n", url.toString().toStdString());
    QNetworkReply *reply = ConnectionManager->get(request);
    QEventLoop loop;

    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();

    QByteArray data = reply->readAll();
    QJsonDocument json = QJsonDocument::fromJson(data);
    QJsonObject root = json.object();

    if ( root.contains("result") && root["result"]=="success" && root.contains("invoices") ) {
        QJsonArray jsonArray = root["invoices"].toObject()["invoice"].toArray();
        for (const QJsonValue & value : jsonArray) {
            QJsonObject obj = value.toObject();
            int invoiceId = obj["id"].toInt();
            QString status = obj["status"].toString();
            QString total = obj["total"].toString() + " " + obj["currencycode"].toString();
            QString duedate = obj["duedate"].toString();
LogPrintf("nodeSetupAPIListInvoices %d, %s, %s \n",invoiceId, status.toStdString(), duedate.toStdString() );
            if ( status == "Unpaid" )   {
                QString description = "#" + QString::number(invoiceId) + " " + duedate + " (" + total + " )";
                ret.insert( { invoiceId , description.toStdString() } );
            }
        }
    }
    else    {
        if ( root.contains("message") )    {
            strError = root["message"].toString();
        }
        else    {
            strError = "ERROR API ListInvoices";
        }
    }

    return ret;
}

QJsonObject MasternodeList::nodeSetupAPIInfo( int serviceid, int clientid, QString email, QString password, QString& strError )  {

    QString Service = QString::fromStdString("info");
    QUrl url( MasternodeList::NODESETUP_ENDPOINT_NODE );
    QUrlQuery urlQuery( url );
    urlQuery.addQueryItem("action", Service);
    urlQuery.addQueryItem("serviceid", QString::number(serviceid));
    urlQuery.addQueryItem("clientid", QString::number(clientid));
    urlQuery.addQueryItem("email", email);
    urlQuery.addQueryItem("password", password);
    url.setQuery( urlQuery );

    QNetworkRequest request( url );
LogPrintf("nodeSetup::Info -- %s\n", url.toString().toStdString());
    QNetworkReply *reply = ConnectionManager->get(request);
    QEventLoop loop;

    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();

    QByteArray data = reply->readAll();
    QJsonDocument json = QJsonDocument::fromJson(data);
    QJsonObject root = json.object();

    return root;
}

bool MasternodeList::nodeSetupAPINodeList( QString email, QString password, QString& strError  )  {
    bool ret = false;

    QString Service = QString::fromStdString("nodelist");
    QUrl url( MasternodeList::NODESETUP_ENDPOINT_BASIC );
    QUrlQuery urlQuery( url );
    urlQuery.addQueryItem("action", Service);
    urlQuery.addQueryItem("email", email);
    urlQuery.addQueryItem("password2", password);
    url.setQuery( urlQuery );

    QNetworkRequest request( url );
LogPrintf("nodeSetup::NodeList -- %s\n", url.toString().toStdString());
    QNetworkReply *reply = ConnectionManager->get(request);
    QEventLoop loop;

    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();

    QByteArray data = reply->readAll();
    QJsonDocument json = QJsonDocument::fromJson(data);
    QJsonObject root = json.object();
    int serviceId;
    QString strAddress;

    if ( root.contains("result") && root["result"]=="success" && root.contains("products") ) {
        QJsonArray jsonArray = root["products"].toObject()["product"].toArray();
        for (const QJsonValue & value : jsonArray) {
            QJsonObject obj = value.toObject();
            serviceId = obj["id"].toInt();
            if ( obj.contains("customfields") )   {
                QJsonArray customfieldsArray = obj["customfields"].toObject()["customfield"].toArray();
                for (const QJsonValue & customfield : customfieldsArray) {
                    QJsonObject field = customfield.toObject();
                    if ( field["name"].toString() == "Address" )    {
                        nodeSetupSetServiceForNodeAddress( field["value"].toString(), serviceId );
                        ret = true;
                        break;
                    }
                }
            }
        }
    }
    else    {
        if ( root.contains("message") )    {
            strError = root["message"].toString();
        }
        else    {
            strError = "ERROR reading NodeList from API";
        }
    }
    return ret;
}

QJsonObject MasternodeList::nodeSetupAPINodeInfo( int serviceid, int clientid, QString email, QString password, QString& strError )  {
    QJsonObject ret;

    QString Service = QString::fromStdString("nodeinfo");
    QUrl url( MasternodeList::NODESETUP_ENDPOINT_NODE );
    QUrlQuery urlQuery( url );
    urlQuery.addQueryItem("action", Service);
    urlQuery.addQueryItem("serviceid", QString::number(serviceid));
    urlQuery.addQueryItem("clientid", QString::number(clientid));
    urlQuery.addQueryItem("email", email);
    urlQuery.addQueryItem("password", password);
    url.setQuery( urlQuery );

    QNetworkRequest request( url );
LogPrintf("nodeSetup::Info -- %s\n", url.toString().toStdString());
    QNetworkReply *reply = ConnectionManager->get(request);
    QEventLoop loop;

    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();

    QByteArray data = reply->readAll();
    QJsonDocument json = QJsonDocument::fromJson(data);
    QJsonObject root = json.object();

    return root;
}

// facilitate reusing burn txs and migration from VPS hosting to nodeSetup hosting
// pick only burn txs less than 1yr old, and not in use by any "ready" DIN node.
std::map<std::string, std::string> MasternodeList::nodeSetupGetUnusedBurnTxs( ) {

    std::map<std::string, std::string> ret;

    CAmount nFee;
    std::string strSentAccount;
    std::list<COutputEntry> listReceived;
    std::list<COutputEntry> listSent;
    isminefilter filter = ISMINE_SPENDABLE;

    std::vector<std::shared_ptr<CWallet>> wallets = GetWallets();
    CWallet * const pwallet = (wallets.size() > 0) ? wallets[0].get() : nullptr;

    if (pwallet==nullptr)   return ret;

    LOCK2(cs_main, pwallet->cs_wallet);

    const CWallet::TxItems & txOrdered = pwallet->wtxOrdered;

    // iterate backwards until we reach >1 yr to return:
    for (CWallet::TxItems::const_reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it)
    {
        CWalletTx *const pwtx = (*it).second.first;
        if (pwtx == nullptr)    continue;

        int confirms = pwtx->GetDepthInMainChain(false);
        if (confirms>720*365)   break;  // expired

        pwtx->GetAmounts(listReceived, listSent, nFee, strSentAccount, filter);
        for (const COutputEntry& s : listSent)
        {
            std::string destAddress="";
            if (IsValidDestination(s.destination)) {
                destAddress = EncodeDestination(s.destination);
            }
            std::string txHash = pwtx->GetHash().GetHex();
            if (destAddress == Params().GetConsensus().cBurnAddress && confirms<720*365 && nodeSetupUsedBurnTxs.find(txHash.substr(0, 16)) == nodeSetupUsedBurnTxs.end() )  {

                std::string description = "";
                std::string strNodeType = "";
                CAmount roundAmount = ((int)(s.amount / COIN)+1);

                if ( roundAmount == Params().GetConsensus().nMasternodeBurnSINNODE_1 )  {
                    strNodeType = "LIL";
                }
                else if ( roundAmount == Params().GetConsensus().nMasternodeBurnSINNODE_5 )  {
                    strNodeType = "MID";
                }
                else if ( roundAmount == Params().GetConsensus().nMasternodeBurnSINNODE_10 )  {
                    strNodeType = "BIG";
                }
                else    {
                    strNodeType = "Unknown";
                }

                description = strNodeType + " " + GUIUtil::dateTimeStr(pwtx->GetTxTime()).toUtf8().constData();
LogPrintf("nodeSetupGetUnusedBurnTxs  confirmed %s, %d, %s \n", txHash.substr(0, 16), roundAmount, description);
                ret.insert( { txHash,  description} );
            }
        }
    }

    return ret;
}

void MasternodeList::nodeSetupStep( std::string icon , std::string text )   {

    std::string strIcon = ":/icons/" + icon;

    labelPic[currentStep]->setVisible(true);
    labelTxt[currentStep]->setVisible(true);
    QPixmap labelIcon ( QString::fromStdString( icon ) );
    labelPic[currentStep]->setPixmap(labelIcon);
    labelTxt[currentStep]->setText( QString::fromStdString( text ) );
}

void MasternodeList::nodeSetupCleanProgress()   {

    for(int idx=0;idx<8;idx++) {
        labelPic[idx]->setVisible(false);
        labelTxt[idx]->setVisible(false);
    }
    currentStep = 0;
}

// RPC helper
UniValue nodeSetupCallRPC(string args)
{
    vector<string> vArgs;
    string uri;

LogPrintf("nodeSetupCallRPC  %s\n", args);

    boost::split(vArgs, args, boost::is_any_of(" \t"));
    string strMethod = vArgs[0];
    vArgs.erase(vArgs.begin());
    //Array params = RPCConvertValues(strMethod, vArgs);
    UniValue params = RPCConvertValues(strMethod, vArgs );

    JSONRPCRequest req;
    req.params = params;
    req.strMethod = strMethod;
    req.URI = uri;
    return ::tableRPC.execute(req);
}
