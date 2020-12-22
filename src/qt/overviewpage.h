// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_OVERVIEWPAGE_H
#define BITCOIN_QT_OVERVIEWPAGE_H

#include <primitives/transaction.h>
#include <interfaces/wallet.h>
#include <wallet/wallet.h>

#include <QWidget>
#include <memory>
#include <QtNetwork/QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

class ClientModel;
class TransactionFilterProxy;
class TxViewDelegate;
class PlatformStyle;
class WalletModel;


namespace Ui {
    class OverviewPage;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
class QNetworkAccessManager;
class QNetworkRequest;
QT_END_NAMESPACE

/** Overview ("home") page widget */
class OverviewPage : public QWidget
{
    Q_OBJECT

public:
    explicit OverviewPage(const PlatformStyle *platformStyle, QWidget *parent = 0);
    ~OverviewPage();

    void setClientModel(ClientModel *clientModel);
    void setWalletModel(WalletModel *walletModel);
    void showOutOfSyncWarning(bool fShow);
    std::vector<COutput> termDepositInfo;

    void showTransactionWidget(bool bShow);
    void showToolBoxWidget(bool bShow);
        
public Q_SLOTS:
    void setBalance(const interfaces::WalletBalances& balances);
    void getPriceInfo();
    void getPriceInfoEur();
        
Q_SIGNALS:
    void transactionClicked(const QModelIndex &index);
    void outOfSyncWarningClicked();
    void sendCoinsClicked(QString addr = "");
    void receiveCoinsClicked();

private:
    Ui::OverviewPage *ui;
    ClientModel *clientModel;
    WalletModel *walletModel;
    QTimer* m_timer;
    QNetworkAccessManager* m_networkManager;
    interfaces::WalletBalances m_balances;
    QTimer *pricingTimer;
    QTimer *pricingTimerEUR;
    QNetworkAccessManager* networkManager;
    QNetworkAccessManager* networkManagerEUR;
    QNetworkRequest* request;
    QNetworkRequest* requestEUR;
    qint64 totalBalance;
    int nDisplayUnit;    

    TxViewDelegate *txdelegate;
    std::unique_ptr<TransactionFilterProxy> filter;

    void SetupTransactionList(int nNumItems);

private Q_SLOTS:
    void updateDisplayUnit();
    void handleTransactionClicked(const QModelIndex &index);
    void updateAlerts(const QString &warnings);
    void updateWatchOnlyLabels(bool showWatchOnly);
    void handleOutOfSyncWarningClicks();
    void onResult(QNetworkReply* replystats);
    void getStatistics();
    void on_buttonSend_clicked();
    void on_buttonReceive_clicked();
    
};

#endif // BITCOIN_QT_OVERVIEWPAGE_H
