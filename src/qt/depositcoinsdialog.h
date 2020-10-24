// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_DEPOSITCOINSDIALOG_H
#define BITCOIN_QT_DEPOSITCOINSDIALOG_H

#include <qt/walletmodel.h>

#include <QDialog>
#include <QMessageBox>
#include <QString>
#include <QTimer>

#define MAX_HCO_VALUE 7500000000000

class ClientModel;
class PlatformStyle;
class SendCoinsEntry;
class SendCoinsRecipient;

namespace Ui {
    class DepositCoinsDialog;
}

QT_BEGIN_NAMESPACE
class QUrl;
QT_END_NAMESPACE

/** Dialog for sending sins */
class DepositCoinsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DepositCoinsDialog(const PlatformStyle *platformStyle, QWidget *parent = 0);
    ~DepositCoinsDialog();

    void setClientModel(ClientModel *clientModel);
    void setModel(WalletModel *model);

    QWidget *setupTabChain(QWidget *prev);

    void setAddress(const QString &address);
    void pasteEntry(const SendCoinsRecipient &rv);
    bool handlePaymentRequest(const SendCoinsRecipient &recipient);

public Q_SLOTS:
    void clear();
    void reject();
    void accept();
    SendCoinsEntry *addEntry();
    void updateTabsAndLabels();
    void setBalance(const interfaces::WalletBalances& balances);

Q_SIGNALS:
    void coinsSent(const uint256& txid);

private:
    Ui::DepositCoinsDialog *ui;
    ClientModel *clientModel;
    WalletModel *model;
    const PlatformStyle *platformStyle;

    // Process WalletModel::SendCoinsReturn and generate a pair consisting
    // of a message and message flags for use in Q_EMIT message().
    // Additional parameter msgArg can be used via .arg(msgArg).
    void processSendCoinsReturn(const WalletModel::SendCoinsReturn &sendCoinsReturn, const QString &msgArg = QString());

private Q_SLOTS:
    void on_sendButton_clicked();
    void updateDisplayUnit();
    void removeEntry(SendCoinsEntry* entry);
Q_SIGNALS:
    // Fired when a message should be reported to the user
    void message(const QString &title, const QString &message, unsigned int style);
};

#define SEND_CONFIRM_DELAY   3

class DepositConfirmationDialog : public QMessageBox
{
    Q_OBJECT

public:
    DepositConfirmationDialog(const QString &title, const QString &text, int secDelay = SEND_CONFIRM_DELAY, QWidget *parent = 0);
    int exec();

private Q_SLOTS:
    void countDown();
    void updateYesButton();

private:
    QAbstractButton *yesButton;
    QTimer countDownTimer;
    int secDelay;
};

#endif // BITCOIN_QT_DEPOSITCOINSDIALOG_H
