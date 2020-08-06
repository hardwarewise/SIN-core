// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2018 FXTC developers
// Copyright (c) 2018-2019 SIN developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef FXTC_QT_MASTERNODELIST_H
#define FXTC_QT_MASTERNODELIST_H

#include <primitives/transaction.h>
#include <qt/platformstyle.h>
#include <sync.h>
#include <util.h>
#include <wallet/wallet.h>
#include <QMenu>
#include <QLabel>
#include <QTimer>
#include <QWidget>
#include <QJsonObject>
#include <univalue.h>

#define MY_MASTERNODELIST_UPDATE_SECONDS                 60
#define MASTERNODELIST_UPDATE_SECONDS                    15
#define MASTERNODELIST_FILTER_COOLDOWN_SECONDS            3

namespace Ui {
    class MasternodeList;
}

class ClientModel;
class WalletModel;
class OptionsModel;
class QNetworkAccessManager;
class UniValue;

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

struct infinitynode_conf_t
{
    std::string IPaddress = "";
    int port = 20970;
    std::string infinitynodePrivateKey;
    std::string collateralHash = "";
    int collateralIndex = 0;
    std::string burnFundHash = "";
    int burnFundIndex = 0;
    CTxDestination collateralAddress{};
};

/** Masternode Manager page widget */
class MasternodeList : public QWidget
{
    Q_OBJECT

public:
    explicit MasternodeList(const PlatformStyle *platformStyle, QWidget *parent = 0);
    ~MasternodeList();

    void setClientModel(ClientModel *clientModel);
    void setWalletModel(WalletModel *walletModel);
    void StartAlias(std::string strAlias);
    void StartAll(std::string strCommand = "start-all");

private:
    QMenu *contextMenu;
    QMenu *contextDINMenu;
    int64_t nTimeFilterUpdated;
    bool fFilterUpdated;

    QLabel* labelPic[8];
    QLabel* labelTxt[8];
    int currentStep = 0;

public Q_SLOTS:
    void updateMyMasternodeInfo(QString strAlias, QString strAddr, const COutPoint& outpoint);
    void updateMyNodeList(bool fForce = false);
    void updateNodeList();
    void updateDINList();

    // node setup functions
    void nodeSetupInitialize();
    void nodeSetupCleanProgress();
    void nodeSetupEnableOrderUI( bool bEnable, int orderID = 0, int invoiceID = 0 );
    int nodeSetupAPIAddClient( QString firstName, QString lastName, QString email, QString password, QString& strError );
    int nodeSetupAPIAddOrder( int clientid, QString billingCycle, QString& productids, int& invoiceid, QString& strError );
    bool nodeSetupAPIGetInvoice( int invoiceid, QString& strAmount, QString& strStatus, QString& paymentAddress, QString& strError );
    std::map<int,std::string> nodeSetupAPIListInvoices( QString email, QString password, QString& strError );
    QJsonObject nodeSetupAPIInfo( int serviceid, int clientid, QString email, QString password, QString& strError );
    QJsonObject nodeSetupAPINodeInfo( int serviceid, int clientId, QString email, QString pass, QString& strError  );
    bool nodeSetupAPINodeList( QString email, QString pass, QString& strError  );
    bool nodeSetupCheckFunds( CAmount invoiceAmount = 0 );
    void nodeSetupStep( std::string icon , std::string text );
    int  nodeSetupGetClientId( QString& email, QString& pass );
    void nodeSetupSetClientId( int clientId, QString email, QString pass );
    void nodeSetupEnableClientId( int clientId );
    int  nodeSetupGetOrderId( int& invoiceid, QString& mProductIds );
    void nodeSetupSetOrderId( int orderid , int invoiceid, QString strProductIds );
    QString  nodeSetupGetBurnTx( );
    void nodeSetupSetBurnTx( QString strBurnTx );
    QString nodeSetupGetPaymentTx( );
    void nodeSetupSetPaymentTx( QString txHash );
    int nodeSetupGetServiceForNodeAddress( QString nodeAdress );
    void nodeSetupSetServiceForNodeAddress( QString nodeAdress, int serviceId );
    void nodeSetupResetClientId( );
    void nodeSetupResetOrderId( );
    void nodeSetupPopulateInvoicesCombo( );
    void nodeSetupPopulateBurnTxCombo( );

    QString nodeSetupCheckInvoiceStatus();
    QString nodeSetupRPCBurnFund( QString collateralAddress, CAmount amount, QString backupAddress );
    int nodeSetupGetBurnAmount();
    QString nodeSetupGetNewAddress();
    UniValue nodeSetupGetTxInfo( QString txHash, std::string attribute);
    QString nodeSetupSendToAddress( QString strAddress, int amount, QTimer* timerConfirms );
    void nodeSetupCheckBurnPrepareConfirmations();
    void nodeSetupCheckBurnSendConfirmations();
    std::map<std::string, std::string> nodeSetupGetUnusedBurnTxs( );

Q_SIGNALS:

private:
    QTimer *timer;
    Ui::MasternodeList *ui;
    ClientModel *clientModel;
    WalletModel *walletModel;

    // Protects tableWidgetMasternodes
    CCriticalSection cs_mnlist;

    // Protects tableWidgetMyMasternodes
    CCriticalSection cs_mymnlist;

    QString strCurrentFilter;
    bool bDINNodeAPIUpdate = false;

    // nodeSetup
    QTimer *invoiceTimer;
    QTimer *burnPrepareTimer;
    QTimer *burnSendTimer;
    QNetworkAccessManager *ConnectionManager;
    QString NODESETUP_ENDPOINT_BASIC;
    QString NODESETUP_ENDPOINT_NODE;
    QString NODESETUP_PID;
    int NODESETUP_CONFIRMS;
    int mClientid, mOrderid, mInvoiceid, mServiceId;
    std::map<std::string, int> nodeSetupUsedBurnTxs;
    QString mPaymentTx;
    QString mBurnPrepareTx;
    QString mBurnAddress;
    QString mBurnTx;
    QString mProductIds;
    std::string billingOptions[3] = {"Monthly", "Semiannually", "Annually"};
    QAction *mCheckNodeAction;

private Q_SLOTS:
    void showContextMenu(const QPoint &);
    void showContextDINMenu(const QPoint &);
    void on_filterLineEdit_textChanged(const QString &strFilterIn);
    void on_startButton_clicked();
    void on_startAllButton_clicked();
    //void on_startAutoSINButton_clicked();
    void on_tableWidgetMyMasternodes_itemSelectionChanged();
    void on_UpdateButton_clicked();
    void on_checkDINNode();
    void on_payButton_clicked();

    // node setup
    void on_btnSetup_clicked();
    void on_btnCheck_clicked();
    void on_btnLogin_clicked();
    void on_btnSetupReset_clicked();
};

#endif // FXTC_QT_MASTERNODELIST_H
