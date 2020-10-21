// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/depositcoinsdialog.h>
#include <qt/forms/ui_depositcoinsdialog.h>

#include <qt/addresstablemodel.h>
#include <qt/sinunits.h>
#include <qt/clientmodel.h>
#include <qt/coincontroldialog.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/sendcoinsentry.h>

#include <chainparams.h>
#include <interfaces/node.h>
#include <key_io.h>
#include <wallet/coincontrol.h>
#include <ui_interface.h>
#include <txmempool.h>
#include <policy/fees.h>
#include <wallet/fees.h>
#include <util.h>

#include <QFontMetrics>
#include <QScrollBar>
#include <QSettings>
#include <QTextDocument>

DepositCoinsDialog::DepositCoinsDialog(const PlatformStyle *_platformStyle, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DepositCoinsDialog),
    clientModel(0),
    model(0),
    platformStyle(_platformStyle)
{
    ui->setupUi(this);

    ui->addButton->hide();
    ui->clearButton->hide();

    if (!_platformStyle->getImagesOnButtons()) {
        ui->sendButton->setIcon(QIcon());
    } else {
        ui->sendButton->setIcon(_platformStyle->SingleColorIcon(":/icons/deposit"));
    }

    addEntry();

    connect(ui->sendButton, SIGNAL(clicked()), this, SLOT(on_sendButton_clicked()));
}

void DepositCoinsDialog::setClientModel(ClientModel *_clientModel)
{
    this->clientModel = _clientModel;

    if (_clientModel) {
        //connect(_clientModel, SIGNAL(numBlocksChanged(int,QDateTime,double,bool)), this, SLOT(updateDisplayUnit()));
    }
}

void DepositCoinsDialog::setModel(WalletModel *_model)
{
    this->model = _model;

    if(_model && _model->getOptionsModel())
    {
        for(int i = 0; i < ui->entries->count(); ++i)
        {
            SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
            if(entry)
            {
                entry->setModel(_model);
                entry->setAsTermDeposit();
            }
        }

        interfaces::WalletBalances balances = _model->wallet().getBalances();
        setBalance(balances);
        connect(_model, SIGNAL(balanceChanged(interfaces::WalletBalances)), this, SLOT(setBalance(interfaces::WalletBalances)));
        connect(_model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
        updateDisplayUnit();
    }
}

DepositCoinsDialog::~DepositCoinsDialog()
{
    delete ui;
}

void DepositCoinsDialog::on_sendButton_clicked()
{
    if(!model || !model->getOptionsModel())
        return;

    QList<SendCoinsRecipient> recipients;
    bool valid = true;

    int termDepositBlocks = 0;
    for(int i = 0; i < ui->entries->count(); ++i)
    {
        SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if(entry)
        {
            if(entry->validate(model->node()))
            {
                recipients.append(entry->getValue());
                termDepositBlocks = entry->getTermDepositLength();
                if (termDepositBlocks < Params().MaxReorganizationDepth() + 1){
                    QString questionString = QString::fromStdString("Number of block is false.");
                    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Hodl Period Error"),
                        questionString,
                        QMessageBox::Yes | QMessageBox::Cancel,
                    QMessageBox::Cancel);
                    return;
                }
            }
            else
            {
                valid = false;
            }
        }
    }

    if(!valid || recipients.isEmpty())
    {
        return;
    }

    WalletModel::UnlockContext ctx(model->requestUnlock());
    if(!ctx.isValid())
    {
        return;
    }

    // prepare transaction for getting txFee earlier
    WalletModelTransaction currentTransaction(recipients);
    WalletModel::SendCoinsReturn prepareStatus;
    std::string termDepositConfirmQuestion = "";
    CCoinControl coinControl;
    coinControl.m_feerate = CFeeRate(DEFAULT_MIN_RELAY_TX_FEE * 2);

    prepareStatus = model->prepareTransaction(currentTransaction, termDepositConfirmQuestion, termDepositBlocks, &coinControl);

    // process prepareStatus and on error generate message shown to user
    processSendCoinsReturn(prepareStatus,
        BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), currentTransaction.getTransactionFee()));

    if(prepareStatus.status != WalletModel::OK) {
        return;
    }

    if(termDepositConfirmQuestion!=""){
        QString questionString = QString::fromStdString(termDepositConfirmQuestion);
        QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm Term Deposit"),
            questionString,
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel);
        if(retval != QMessageBox::Yes)
        {
            return;
        }
    }else{
        QString questionString = QString::fromStdString("Something went wrong! No term deposit instruction was detected. Instruction will be cancelled.");
        QMessageBox::StandardButton retval = QMessageBox::question(this, tr("No Term Deposit Detected"),
            questionString,
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel);
        return;
    }

    CAmount txFee = currentTransaction.getTransactionFee();

    // Format confirmation message
    QStringList formatted;
    for (const SendCoinsRecipient &rcp : currentTransaction.getRecipients())
    {
        // generate bold amount string with wallet name in case of multiwallet
        QString amount = "<b>" + BitcoinUnits::formatHtmlWithUnit(model->getOptionsModel()->getDisplayUnit(), rcp.amount);
        if (model->isMultiwallet()) {
            amount.append(" <u>"+tr("from wallet %1").arg(GUIUtil::HtmlEscape(model->getWalletName()))+"</u> ");
        }
        amount.append("</b>");
        // generate monospace address string
        QString address = "<span style='font-family: monospace;'>" + rcp.address;
        address.append("</span>");

        QString recipientElement;
        recipientElement = "<br />";

        if (!rcp.paymentRequest.IsInitialized()) // normal payment
        {
            if(rcp.label.length() > 0) // label with address
            {
                recipientElement.append(tr("%1 to %2").arg(amount, GUIUtil::HtmlEscape(rcp.label)));
                recipientElement.append(QString(" (%1)").arg(address));
            }
            else // just address
            {
                recipientElement.append(tr("%1 to %2").arg(amount, address));
            }
        }
        else if(!rcp.authenticatedMerchant.isEmpty()) // authenticated payment request
        {
            recipientElement.append(tr("%1 to %2").arg(amount, GUIUtil::HtmlEscape(rcp.authenticatedMerchant)));
        }
        else // unauthenticated payment request
        {
            recipientElement.append(tr("%1 to %2").arg(amount, address));
        }

        formatted.append(recipientElement);
    }

    QString questionString = tr("Are you sure you want to send?");
    questionString.append("<br /><span style='font-size:10pt;'>");
    questionString.append(tr("Please, review your transaction."));
    questionString.append("</span><br />%1");

    if(txFee > 0)
    {
        // append fee string if a fee is required
        questionString.append("<hr /><b>");
        questionString.append(tr("Transaction fee"));
        questionString.append("</b>");

        // append transaction size
        questionString.append(" (" + QString::number((double)currentTransaction.getTransactionSize() / 1000) + " kB): ");

        // append transaction fee value
        questionString.append("<span style='color:#aa0000; font-weight:bold;'>");
        questionString.append(BitcoinUnits::formatHtmlWithUnit(model->getOptionsModel()->getDisplayUnit(), txFee));
        questionString.append("</span><br />");

        // append RBF message according to transaction's signalling
        questionString.append("<span style='font-size:10pt; font-weight:normal;'>");
        questionString.append("</span>");
    }

    // add total amount in all subdivision units
    questionString.append("<hr />");
    CAmount totalAmount = currentTransaction.getTotalTransactionAmount() + txFee;
    if (totalAmount > MAX_HCO_VALUE) {
        QString questionString = QString::fromStdString("You cannot send more than 75k SIN for the HCO");
        QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Invalid amount for HCO"),
            questionString,
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel);
        return;
    }
    QStringList alternativeUnits;
    for (BitcoinUnits::Unit u : BitcoinUnits::availableUnits())
    {
        if(u != model->getOptionsModel()->getDisplayUnit())
            alternativeUnits.append(BitcoinUnits::formatHtmlWithUnit(u, totalAmount));
    }
    questionString.append(QString("<b>%1</b>: <b>%2</b>").arg(tr("Total Amount"))
        .arg(BitcoinUnits::formatHtmlWithUnit(model->getOptionsModel()->getDisplayUnit(), totalAmount)));
    questionString.append(QString("<br /><span style='font-size:10pt; font-weight:normal;'>(=%1)</span>")
        .arg(alternativeUnits.join(" " + tr("or") + " ")));

    DepositConfirmationDialog confirmationDialog(tr("Confirm send coins"),
        questionString.arg(formatted.join("<br />")), SEND_CONFIRM_DELAY, this);
    confirmationDialog.exec();
    QMessageBox::StandardButton retval = static_cast<QMessageBox::StandardButton>(confirmationDialog.result());

    if(retval != QMessageBox::Yes)
    {
        return;
    }

    // now send the prepared transaction
    WalletModel::SendCoinsReturn sendStatus = model->sendCoins(currentTransaction);
    // process sendStatus and on error generate message shown to user
    processSendCoinsReturn(sendStatus);

    if (sendStatus.status == WalletModel::OK)
    {
        accept();
        Q_EMIT coinsSent(currentTransaction.getWtx()->get().GetHash());
    }
}

void DepositCoinsDialog::clear()
{
    // Remove entries until only one left
    while(ui->entries->count())
    {
        ui->entries->takeAt(0)->widget()->deleteLater();
    }
    addEntry();

    updateTabsAndLabels();
}

void DepositCoinsDialog::reject()
{
    clear();
}

void DepositCoinsDialog::accept()
{
    clear();
}

SendCoinsEntry *DepositCoinsDialog::addEntry()
{
    SendCoinsEntry *entry = new SendCoinsEntry(platformStyle, this);
    entry->setModel(model);
    entry->setAsTermDeposit();
    ui->entries->addWidget(entry);
    connect(entry, SIGNAL(removeEntry(SendCoinsEntry*)), this, SLOT(removeEntry(SendCoinsEntry*)));
    connect(entry, SIGNAL(useAvailableBalance(SendCoinsEntry*)), this, SLOT(useAvailableBalance(SendCoinsEntry*)));

    entry->clear();
    entry->setFocus();
    ui->scrollAreaWidgetContents->resize(ui->scrollAreaWidgetContents->sizeHint());
    qApp->processEvents();
    QScrollBar* bar = ui->scrollArea->verticalScrollBar();
    if(bar)
        bar->setSliderPosition(bar->maximum());

    updateTabsAndLabels();
    return entry;
}

void DepositCoinsDialog::updateTabsAndLabels()
{
    setupTabChain(0);
}

void DepositCoinsDialog::removeEntry(SendCoinsEntry* entry)
{
    entry->hide();

    // If the last entry is about to be removed add an empty one
    if (ui->entries->count() == 1)
        addEntry();

    entry->deleteLater();

    updateTabsAndLabels();
}

QWidget *DepositCoinsDialog::setupTabChain(QWidget *prev)
{
    for(int i = 0; i < ui->entries->count(); ++i)
    {
        SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if(entry)
        {
            prev = entry->setupTabChain(prev);
        }
    }
    QWidget::setTabOrder(prev, ui->sendButton);
    return ui->addButton;
}

void DepositCoinsDialog::setAddress(const QString &address)
{
    SendCoinsEntry *entry = 0;
    // Replace the first entry if it is still unused
    if(ui->entries->count() == 1)
    {
        SendCoinsEntry *first = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(0)->widget());
        if(first->isClear())
        {
            entry = first;
        }
    }
    if(!entry)
    {
        entry = addEntry();
    }
    entry->setAsTermDeposit();
    entry->setAddress(address);
}

void DepositCoinsDialog::pasteEntry(const SendCoinsRecipient &rv)
{
    SendCoinsEntry *entry = 0;
    // Replace the first entry if it is still unused
    if(ui->entries->count() == 1)
    {
        SendCoinsEntry *first = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(0)->widget());
        if(first->isClear())
        {
            entry = first;
        }
    }
    if(!entry)
    {
        entry = addEntry();
    }
    entry->setAsTermDeposit();
    entry->setValue(rv);
    updateTabsAndLabels();
}

bool DepositCoinsDialog::handlePaymentRequest(const SendCoinsRecipient &rv)
{
    // Just paste the entry, all pre-checks
    // are done in paymentserver.cpp.
    pasteEntry(rv);
    return true;
}

void DepositCoinsDialog::setBalance(const interfaces::WalletBalances& balances)
{
    if(model && model->getOptionsModel())
    {
        
        ui->labelBalance->setText(BitcoinUnits::floorHtmlWithUnit(model->getOptionsModel()->getDisplayUnit(), balances.balance, false, BitcoinUnits::separatorAlways));
        std::vector<COutput> vTimeLockInfo = model->wallet().GetTimeLockInfo();

        ui->hodlTable->setRowCount(vTimeLockInfo.size());

        ui->hodlTable->setSortingEnabled(false);

        uint64_t totalLocked  = 0;
        uint64_t totalMatured = 0;

        interfaces::Node& node = clientModel->node();
        int curHeight = node.getNumBlocks();
        int k=0;
        for(auto &out : vTimeLockInfo){
            CTxOut txOut = out.tx->tx->vout[out.i];

            int lockedHeight=curHeight-out.nDepth;
            int releaseBlock=txOut.scriptPubKey.GetTimeLockReleaseBlock();
            int locktimes =releaseBlock-lockedHeight;
            int blocksRemaining=releaseBlock-curHeight;
            int blocksSoFar=curHeight-lockedHeight;

            if(curHeight>=releaseBlock){
                ui->hodlTable->setItem(k, 0, new QTableWidgetItem(QString("Matured")));
                totalMatured += txOut.nValue;
            }else{
                ui->hodlTable->setItem(k, 0, new QTableWidgetItem(QString("Hodl Period")));
                totalLocked  += txOut.nValue;
            }

            ui->hodlTable->setItem(k, 1, new QTableWidgetItem(BitcoinUnits::format(model->getOptionsModel()->getDisplayUnit(), txOut.nValue)));
            ui->hodlTable->setItem(k, 2, new QTableWidgetItem(QString::number(lockedHeight)));
            ui->hodlTable->setItem(k, 3, new QTableWidgetItem(QString::number(releaseBlock)));
            ui->hodlTable->setItem(k, 4, new QTableWidgetItem(QString::number(locktimes)));
            time_t rawtime;
            struct tm * timeinfo;
            char buffer[80];
            time (&rawtime);
            rawtime+=blocksRemaining*120;		// seconds per block
            timeinfo = localtime(&rawtime);
            strftime(buffer,80,"%Y/%m/%d",timeinfo);
            std::string str(buffer);

            ui->hodlTable->setItem(k, 5, new QTableWidgetItem(QString(buffer)));
            k++;
        }
    }
}

void DepositCoinsDialog::updateDisplayUnit()
{
    setBalance(model->wallet().getBalances());
}

void DepositCoinsDialog::processSendCoinsReturn(const WalletModel::SendCoinsReturn &sendCoinsReturn, const QString &msgArg)
{
    QPair<QString, CClientUIInterface::MessageBoxFlags> msgParams;
    // Default to a warning message, override if error message is needed
    msgParams.second = CClientUIInterface::MSG_WARNING;

    // This comment is specific to DepositCoinsDialog usage of WalletModel::SendCoinsReturn.
    // WalletModel::TransactionCommitFailed is used only in WalletModel::sendCoins()
    // all others are used only in WalletModel::prepareTransaction()
    switch(sendCoinsReturn.status)
    {
    case WalletModel::InvalidAddress:
        msgParams.first = tr("The recipient address is not valid. Please recheck.");
        break;
    case WalletModel::InvalidAmount:
        msgParams.first = tr("The amount to pay must be larger than 0.");
        break;
    case WalletModel::AmountExceedsBalance:
        msgParams.first = tr("The amount exceeds your balance.");
        break;
    case WalletModel::AmountWithFeeExceedsBalance:
        msgParams.first = tr("The total exceeds your balance when the %1 transaction fee is included.").arg(msgArg);
        break;
    case WalletModel::DuplicateAddress:
        msgParams.first = tr("Duplicate address found: addresses should only be used once each.");
        break;
    case WalletModel::TransactionCreationFailed:
        msgParams.first = tr("Transaction creation failed!");
        msgParams.second = CClientUIInterface::MSG_ERROR;
        break;
    case WalletModel::TransactionCommitFailed:
        msgParams.first = tr("The transaction was rejected with the following reason: %1").arg(sendCoinsReturn.reasonCommitFailed);
        msgParams.second = CClientUIInterface::MSG_ERROR;
        break;
    case WalletModel::AbsurdFee:
        msgParams.first = tr("A fee higher than %1 is considered an absurdly high fee.").arg(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), model->node().getMaxTxFee()));
        break;
    case WalletModel::PaymentRequestExpired:
        msgParams.first = tr("Payment request expired.");
        msgParams.second = CClientUIInterface::MSG_ERROR;
        break;
    // included to prevent a compiler warning.
    case WalletModel::OK:
    default:
        return;
    }

    Q_EMIT message(tr("Deposit Coins"), msgParams.first, msgParams.second);
}

DepositConfirmationDialog::DepositConfirmationDialog(const QString &title, const QString &text, int _secDelay,
    QWidget *parent) :
    QMessageBox(QMessageBox::Question, title, text, QMessageBox::Yes | QMessageBox::Cancel, parent), secDelay(_secDelay)
{
    setDefaultButton(QMessageBox::Cancel);
    yesButton = button(QMessageBox::Yes);
    updateYesButton();
    connect(&countDownTimer, SIGNAL(timeout()), this, SLOT(countDown()));
}

int DepositConfirmationDialog::exec()
{
    updateYesButton();
    countDownTimer.start(1000);
    return QMessageBox::exec();
}

void DepositConfirmationDialog::countDown()
{
    secDelay--;
    updateYesButton();

    if(secDelay <= 0)
    {
        countDownTimer.stop();
    }
}

void DepositConfirmationDialog::updateYesButton()
{
    if(secDelay > 0)
    {
        yesButton->setEnabled(false);
        yesButton->setText(tr("Yes") + " (" + QString::number(secDelay) + ")");
    }
    else
    {
        yesButton->setEnabled(true);
        yesButton->setText(tr("Yes"));
    }
}
