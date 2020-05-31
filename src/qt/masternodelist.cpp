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
#include <qt/clientmodel.h>
#include <qt/optionsmodel.h>
#include <qt/guiutil.h>
#include <init.h>
#include <key_io.h>
#include <core_io.h>
#include <masternode-sync.h>
#include <masternodeconfig.h>
#include <masternodeman.h>
#include <sync.h>
#include <wallet/wallet.h>
#include <qt/walletmodel.h>

#include <QDialog>
#include <QInputDialog>
#include <QTimer>
#include <QMessageBox>
#include <QStyleFactory>

// begin nodeSetup
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkReply>
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

    ui->tableWidgetMyMasternodes->setContextMenuPolicy(Qt::CustomContextMenu);

    QAction *startAliasAction = new QAction(tr("Start alias"), this);
    contextMenu = new QMenu();
    contextMenu->addAction(startAliasAction);
    connect(ui->tableWidgetMyMasternodes, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showContextMenu(const QPoint&)));
    connect(startAliasAction, SIGNAL(triggered()), this, SLOT(on_startButton_clicked()));

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateNodeList()));
    connect(timer, SIGNAL(timeout()), this, SLOT(updateMyNodeList()));
    timer->start(1000);

    fFilterUpdated = false;
    nTimeFilterUpdated = GetTime();
    updateNodeList();

    // node setup
    NODESETUP_ENDPOINT = QString::fromStdString(gArgs.GetArg("-nodesetupurl", "https://setup.sinovate.io/includes/api/basic.php"));
    nodeSetupInitialize();
}

MasternodeList::~MasternodeList()
{
    delete ui;
    delete ConnectionManager;
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

//void MasternodeList::on_startAutoSINButton_clicked()
//{
    //std::vector<std::shared_ptr<CWallet>> wallets = GetWallets();
    //CWallet * const pwallet = (wallets.size() > 0) ? wallets[0].get() : nullptr;
    //LOCK2(cs_main, pwallet->cs_wallet);
    //bool ok;
    //setStyleSheet( "QDialog{ background-color: #0d1827; }");
    //QString vpsip = QInputDialog::getText(this, tr("SINnode"), tr("Enter VPS address:"), QLineEdit::Normal, "", &ok);

    //if (!ok)
       //return;

    //Write in file
    //boost::filesystem::path pathMasternodeConfigFile = GetMasternodeConfigFile();
    //boost::filesystem::ifstream streamConfig(pathMasternodeConfigFile);
    //FILE* configFile = fopen(pathMasternodeConfigFile.string().c_str(), "w");
    //std::string strHeader = "# infinitynode config file\n"
                            //"# Format: alias IP:port infinitynodeprivkey collateral_output_txid collateral_output_index burnfund_output_txid burnfund_output_index\n"
                            //"# infinitynode1 127.0.0.1:20980 7RVuQhi45vfazyVtskTRLBgNuSrYGecS5zj2xERaooFVnWKKjhS b7ed8c1396cf57ac78d756186b6022d3023fd2f1c338b7fbae42d342fdd7070a 0 563d9434e816b3e8ffc5347c6b8db07509de6068f6759f21a16be5d92b7e3111 1\n";
    //fwrite(strHeader.c_str(), std::strlen(strHeader.c_str()), 1, configFile);

    //LogPrintf("MasternodeList::AutoSIN -- location of configFile is %s\n",pathMasternodeConfigFile.string());
    // quick input parsing
    //int vpsiplen = strlen(vpsip.toUtf8().constData());
    //if (vpsiplen < 7 || vpsiplen > 16) {
       //strHeader = "#IP format is not valid\n";
       //fwrite(strHeader.c_str(), std::strlen(strHeader.c_str()), 1, configFile);
       //fclose(configFile);
       //return;
    //}
    // char type parsing
    //for (int i=0; i<vpsiplen; i++) {
       //if ((vpsip[i] < 46 || vpsip[i] > 57) || vpsip[i] == 47) {
           //strHeader = "#IP format is not valid\n";
           //fwrite(strHeader.c_str(), std::strlen(strHeader.c_str()), 1, configFile);
           //fclose(configFile);
           //return;
       //}
    //}

    // generate masternode key
    //CKey secret;
    //std::vector<infinitynode_conf_t> listNode;
    //std::set<uint256> trackCollateralTx;

    //secret.MakeNewKey(false);

    //bool foundCollat = false;
    //CTxDestination collateralAddress = CTxDestination();

    // find suitable burntx
    //bool foundBurn = false;
    //int counter = 0;
    //listNode.clear();

    //for (map<uint256, CWalletTx>::const_iterator it = pwallet->mapWallet.begin(); it != pwallet->mapWallet.end(); ++it) {
      //const uint256* txid = &(*it).first;
      //const CWalletTx* pcoin = &(*it).second;
      //for (unsigned int i = 0; i < pcoin->tx->vout.size(); i++) {
          //CTxDestination address;
          //bool fValidAddress = ExtractDestination(pcoin->tx->vout[i].scriptPubKey, address);
          //CTxDestination BurnAddress = DecodeDestination(Params().GetConsensus().cBurnAddress);
          //if (
                //(address == BurnAddress) &&
                //(
                    //((Params().GetConsensus().nMasternodeBurnSINNODE_1 - 1) * COIN < pcoin->tx->vout[i].nValue && pcoin->tx->vout[i].nValue <= Params().GetConsensus().nMasternodeBurnSINNODE_1 * COIN) ||
                    //((Params().GetConsensus().nMasternodeBurnSINNODE_5 - 1) * COIN < pcoin->tx->vout[i].nValue && pcoin->tx->vout[i].nValue <= Params().GetConsensus().nMasternodeBurnSINNODE_5 * COIN) ||
                    //((Params().GetConsensus().nMasternodeBurnSINNODE_10 - 1) * COIN < pcoin->tx->vout[i].nValue && pcoin->tx->vout[i].nValue <= Params().GetConsensus().nMasternodeBurnSINNODE_10 * COIN)
                //)
          //) {
                    //add dummy
                    //listNode.push_back(infinitynode_conf_t());
                    //foundBurn = true;

                    //add to list
                    //listNode[counter].burnFundHash = txid->ToString();
                    //listNode[counter].burnFundIndex = i;
                    //const CTxIn txin = pcoin->tx->vin[0]; //BurnFund Input is only one address. So we can take the first without problem
                    //CTxDestination sendAddress;
                    //ExtractDestination(pwallet->mapWallet.at(txin.prevout.hash).tx->vout[txin.prevout.n].scriptPubKey, sendAddress);
                    //LogPrintf("MasternodeList::AutoSIN -- find BurnFund tx: %s, index: %d\n",listNode[counter].burnFundHash, listNode[counter].burnFundIndex);
                    //LogPrintf("MasternodeList::AutoSIN -- Sender's address:%s\n", EncodeDestination(sendAddress));
                    //listNode[counter].collateralAddress = sendAddress;
                    //secret.MakeNewKey(false);
                    //listNode[counter].infinitynodePrivateKey = EncodeSecret(secret);
                    //listNode[counter].IPaddress = vpsip.toUtf8().constData();
                    //listNode[counter].port = Params().GetDefaultPort();
                    //counter++;
            //}
        //}
    //}

    // find suitable collateral outputs
    //std::vector<COutput> vPossibleCoins;
    //pwallet->AvailableCoins(vPossibleCoins, true, NULL, false, ONLY_MASTERNODE_COLLATERAL);

    //for (COutput& out : vPossibleCoins) {
      //CTxDestination address;
        //const CScript& scriptPubKey = out.tx->tx->vout[out.i].scriptPubKey;
        //bool fValidAddress = ExtractDestination(scriptPubKey, address);
        //for (unsigned int i = 0; i < listNode.size(); i++) {
            //if (address == listNode[i].collateralAddress && trackCollateralTx.count(out.tx->GetHash()) != 1) {
                //listNode[i].collateralHash = out.tx->GetHash().ToString();
                //listNode[i].collateralIndex = out.i;
                //trackCollateralTx.insert(out.tx->GetHash());
                //foundCollat = true;
            //}
        //}
    //}

    //if (!foundBurn) {
        //LogPrintf("MasternodeList::AutoSIN -- burnTx not found\n");
        //strHeader = "#BurnFund tx was not found\n";
        //fwrite(strHeader.c_str(), std::strlen(strHeader.c_str()), 1, configFile);
    //} else {
        //strHeader = "#BurnFund tx was found\n";
        //fwrite(strHeader.c_str(), std::strlen(strHeader.c_str()), 1, configFile);
    //}

    //if (!foundCollat) {
        //LogPrintf("MasternodeList::AutoSIN -- collateral not found\n");
        //strHeader = "#Collateral tx was not found\n";
        //fwrite(strHeader.c_str(), std::strlen(strHeader.c_str()), 1, configFile);
    //} else {
        //strHeader = "#Collateral tx was found\n";
        //fwrite(strHeader.c_str(), std::strlen(strHeader.c_str()), 1, configFile);
    //}

    //if (foundBurn) {
       //for (unsigned int i = 0; i < listNode.size(); i++) {
           //char inconfigline[300];
           //memset(inconfigline,'\0',300);
           //sprintf(inconfigline,"infinitynode%d %s:%d %s %s %d %s %d %s\n",i, listNode[i].IPaddress.c_str(), listNode[i].port, listNode[i].infinitynodePrivateKey.c_str(), listNode[i].collateralHash.c_str(), listNode[i].collateralIndex, listNode[i].burnFundHash.c_str(), listNode[i].burnFundIndex, EncodeDestination(listNode[i].collateralAddress).c_str());
           //fwrite(inconfigline, strlen(inconfigline), 1, configFile);
       //}
    //}

    ////////////////////////////////////////////////////////////////////////////////
    //QMessageBox messageBox;
    //messageBox.setWindowTitle("Alert");
    //messageBox.setText(QString::fromStdString("Please open config file in:\n"+pathMasternodeConfigFile.string()));
    //messageBox.exec();
    ////////////////////////////////////////////////////////////////////////////////

    //fclose(configFile);
//}

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

// nodeSetup buttons
void MasternodeList::on_btnCheck_clicked()
{
    nodeSetupCleanProgress();
    if ( !nodeSetupCheckFunds() )   {
        ui->labelMessage->setText("You didn't pass the checks. Please review.");
        return;
    }

    // TODO continue
    ui->labelMessage->setText("You passed all checks. Please select a billing period and press Setup to continue.");
    ui->btnSetup->setEnabled(true);
    return;
}

void MasternodeList::on_btnSetup_clicked()
{
    // check again in case they changed the tier...
    nodeSetupCleanProgress();
    if ( !nodeSetupCheckFunds() )   {
        ui->labelMessage->setText("You didn't pass the checks. Please review.");
        return;
    }

    int orderid, invoiceid, productid;
    QString strError;
    QString strBillingCycle = QString::fromStdString(billingOptions[ui->comboBilling->currentData().toInt()]);

//LogPrintf("place order %d, %s ", mClientid, strBillingCycle);
    mOrderid = nodeSetupAPIAddOrder( mClientid, strBillingCycle, mProductIds, mInvoiceid, strError );
    if ( mOrderid > 0 && mInvoiceid > 0) {
        nodeSetupSetOrderId( mOrderid, mInvoiceid, mProductIds );
        nodeSetupEnableOrderUI(true, mOrderid, mInvoiceid);
        ui->labelMessage->setText(QString::fromStdString(strprintf("Order placed successfully. Order ID #%d Invoice ID #%d", mOrderid, mInvoiceid)));

        // get invoice data and do payment
        QString strAmount, strStatus, paymentAddress;
        nodeSetupAPIGetInvoice( mInvoiceid, strAmount, strStatus, paymentAddress, strError );
        CAmount invoiceAmount = strAmount.toDouble() * COIN;
ui->labelMessage->setText(QString::fromStdString(strprintf("Check for invoice amount #%d", invoiceAmount)));
        if ( nodeSetupCheckFunds( invoiceAmount ) ) {
            nodeSetupStep( "setupWait", "Paying invoice");
        }
        else {
            nodeSetupStep( "setupKo", "Process stopped");
        }
        currentStep++;
    }
    else    {
        ui->labelMessage->setText(strError);
    }
}


void MasternodeList::on_btnLogin_clicked()
{
    QString strError = "";

    if ( mClientid > 0 )    {   // reset
        nodeSetupResetClientId();
    }
    else {      // create/login
        int clientId = nodeSetupAPIAddClient( ui->txtFirstName->text(), ui->txtLastName->text(), ui->txtEmail->text(), ui->txtPassword->text(), strError );
LogPrintf("nodeSetup API client id %d\n", clientId);
        if ( strError != "" )  {
            ui->labelMessage->setText( strError );
        }

        if ( clientId > 0 ) {
            nodeSetupEnableClientId( clientId );
            nodeSetupSetClientId( clientId, ui->txtEmail->text(), ui->txtPassword->text() );
LogPrintf("nodeSetup enable %d\n", clientId);
        }
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
        ui->labelClientId->setText("");
    }
    else {
        nodeSetupEnableClientId(clientId);
    }

    mOrderid = nodeSetupGetOrderId( mInvoiceid, mProductIds );
    if ( mOrderid > 0 )    {
        ui->labelMessage->setText(QString::fromStdString(strprintf("There is an order ongoing (#%d). Press 'Continue' or 'Reset' order.", mOrderid)));
        nodeSetupEnableOrderUI(true, mOrderid, mInvoiceid);
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
    ui->btnCheck->setEnabled(true);
    ui->labelMessage->setText("Select a node Tier and then press 'Check' to verify if you meet the prerequisites");
    mOrderid = mInvoiceid = 0;
}

void MasternodeList::nodeSetupEnableClientId( int clientId )  {
    ui->widgetLogin->hide();
    ui->labelClientId->setText("#"+QString::number(clientId));
    ui->btnCheck->setEnabled(true);
    ui->labelMessage->setText("Select a node Tier and then press 'Check' to verify if you meet the prerequisites");
    mClientid = clientId;
    ui->btnLogin->setText("Logout");
}

bool MasternodeList::nodeSetupCheckFunds( CAmount invoiceAmount )   {

    bool bRet = false;
    int nMasternodeCollateral = 1; //Params().GetConsensus().nMasternodeCollateralMinimum;
    int nMasternodeBurn = 0;

    if ( ui->radioLILNode->isChecked() )    nMasternodeBurn = 5; //Params().GetConsensus().nMasternodeBurnSINNODE_1;
    if ( ui->radioMIDNode->isChecked() )    nMasternodeBurn = Params().GetConsensus().nMasternodeBurnSINNODE_5;
    if ( ui->radioBIGNode->isChecked() )    nMasternodeBurn = Params().GetConsensus().nMasternodeBurnSINNODE_10;

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


int MasternodeList::nodeSetupAPIAddClient( QString firstName, QString lastName, QString email, QString password, QString& strError )  {
    int ret = 0;

    QString Service = QString::fromStdString("AddClient");
    QUrl url( MasternodeList::NODESETUP_ENDPOINT );
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
    QUrl url( MasternodeList::NODESETUP_ENDPOINT );
    QUrlQuery urlQuery( url );
    urlQuery.addQueryItem("action", Service);
    urlQuery.addQueryItem("clientid", QString::number(clientid));
    urlQuery.addQueryItem("pid", "22");
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
    QUrl url( MasternodeList::NODESETUP_ENDPOINT );
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

    if ( json.object().contains("result") ) {
        if ( json.object()["result"]=="success" ) {
            if ( json.object().contains("description") ) {
                strAmount = json.object()["description"].toString();
            }
            if ( json.object().contains("status") ) {
                strStatus = json.object()["status"].toString();
            }
            if ( json.object().contains("transid") ) {
                paymentAddress = json.object()["transid"].toInt();
            }
            ret = true;
        }
        else    {
            if ( json.object().contains("message") )    {
                strError = json.object()["message"].toString();
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

