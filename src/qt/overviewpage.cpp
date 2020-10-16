// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/overviewpage.h>
#include <qt/forms/ui_overviewpage.h>

#include <qt/sinunits.h>
#include <qt/clientmodel.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/transactionfilterproxy.h>
#include <qt/transactiontablemodel.h>
#include <qt/utilitydialog.h>
#include <qt/walletmodel.h>
#include <wallet/wallet.h>
#include <validation.h>
#include <interfaces/node.h>
#include <init.h>
#include <util.h>
#include <shutdown.h>
#include <instantx.h>
#include <masternode-sync.h>

#include <QAbstractItemDelegate>
#include <QPainter>
#include <QSettings>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>


#define ICON_OFFSET 16
#define DECORATION_SIZE 38
#define NUM_ITEMS 11
#define NUM_ITEMS_ADV 7

Q_DECLARE_METATYPE(interfaces::WalletBalances)

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    explicit TxViewDelegate(const PlatformStyle *_platformStyle, QObject *parent=nullptr):
        QAbstractItemDelegate(parent), unit(BitcoinUnits::SIN),
        platformStyle(_platformStyle)
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(TransactionTableModel::RawDecorationRole));
        QRect mainRect = option.rect;
        mainRect.moveLeft(ICON_OFFSET);
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE - 6, DECORATION_SIZE - 6));
        int xspace = DECORATION_SIZE + 8;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace - ICON_OFFSET, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);
        icon = platformStyle->SingleColorIcon(icon);
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
        if(value.canConvert<QBrush>())
        {
            QBrush brush = qvariant_cast<QBrush>(value);
            foreground = brush.color();
        }

        painter->setPen(foreground);
        QRect boundingRect;
        painter->drawText(addressRect, Qt::AlignLeft|Qt::AlignVCenter, address, &boundingRect);

        if (index.data(TransactionTableModel::WatchonlyRole).toBool())
        {
            QIcon iconWatchonly = qvariant_cast<QIcon>(index.data(TransactionTableModel::WatchonlyDecorationRole));
            QRect watchonlyRect(boundingRect.right() + 5, mainRect.top()+ypad+halfheight, 16, halfheight);
            iconWatchonly.paint(painter, watchonlyRect);
        }

        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if(!confirmed)
        {
            foreground = COLOR_UNCONFIRMED;
        }
        else
        {
            
            foreground = QColor(61, 113, 193);
        }


        painter->setPen(foreground);


        QString amountText = BitcoinUnits::formatWithUnit(unit, amount, true, BitcoinUnits::separatorAlways);
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight|Qt::AlignVCenter, amountText);

        painter->setPen(option.palette.color(QPalette::Text));
        foreground = QColor(205, 220, 234);
        painter->drawText(amountRect, Qt::AlignLeft|Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        
        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        return QSize(DECORATION_SIZE, DECORATION_SIZE);
    }

    int unit;
    const PlatformStyle *platformStyle;

};
#include <qt/overviewpage.moc>

OverviewPage::OverviewPage(const PlatformStyle *platformStyle, QWidget *parent) :
    QWidget(parent),
    // ++ Explorer Stats
    m_timer(nullptr),
    // --
    ui(new Ui::OverviewPage),
    m_networkManager(new QNetworkAccessManager(this)),
    clientModel(0),
    walletModel(0),
    txdelegate(new TxViewDelegate(platformStyle, this))
{
    pricingTimer = new QTimer();
    networkManager = new QNetworkAccessManager();
    request = new QNetworkRequest();
    ui->setupUi(this);

    // ++ Explorer Stats
    m_timer = new QTimer(this);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(getStatistics()));
    m_timer->start(30000);
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(onResult(QNetworkReply*)));
    getStatistics();
    // --

    // Set the USD pricing information
       

        // Network request code 
        QObject::connect(networkManager, &QNetworkAccessManager::finished,
                         this, [=](QNetworkReply *reply) {  
                         
                    if (reply->error()) {
                        ui->labelCurrentPrice->setText("");
                        qDebug() << reply->errorString();
                        return;
                    }
                    // Get the data from the network request
                    QString answer = reply->readAll();

                    // Create regex expression to find the value with 8 decimals
                    QRegExp rx("\\d*.\\d\\d\\d\\d\\d\\d\\d\\d");
                    rx.indexIn(answer);

                    // List the found values
                    QStringList list = rx.capturedTexts();

                    QString currentPriceStyleSheet = ".QLabel{color: %1; font-size:18px;}";
                    // Evaluate the current and next numbers and assign a color (green for positive, red for negative)
                    bool ok;
                    if (!list.isEmpty()) {
                        double next = list.first().toDouble(&ok);
                        if (!ok) {
                            ui->labelCurrentPrice->setStyleSheet(currentPriceStyleSheet.arg("#4960ad"));
                            ui->labelCurrentPrice->setText("");
                        } else {
                            double current = ui->labelCurrentPrice->text().toDouble(&ok);
                            if (!ok) {
                                current = 0.00000000;
                            } else {
                                if (next < current)
                                    ui->labelCurrentPrice->setStyleSheet(currentPriceStyleSheet.arg("red"));
                                else if (next > current)
                                    ui->labelCurrentPrice->setStyleSheet(currentPriceStyleSheet.arg("green"));
                                else
                                    ui->labelCurrentPrice->setStyleSheet(currentPriceStyleSheet.arg("black"));
                                    
                            }
                            ui->labelCurrentPrice->setText(QString("%1").arg(QString().setNum(next, 'f', 8)));
                            //ui->labelCurrentPrice->setToolTip(tr("Brought to you by coinmarketcap.com"));

                            QString total;
                            double current2 = (current * totalBalance / 100000000);
                            total = QString::number(current2, 'f', 2);
                            ui->labelUSDTotal->setText("$" + total + " USD");
                        }
                    }
                }
        );

       

    m_balances.balance = -1;

    // use a SingleColorIcon for the "out of sync warning" icon
    QIcon icon = platformStyle->SingleColorIcon(":/icons/warning");
    icon.addPixmap(icon.pixmap(QSize(24,24), QIcon::Normal), QIcon::Disabled); // also set the disabled icon because we are using a disabled QPushButton to work around missing HiDPI support of QLabel (https://bugreports.qt.io/browse/QTBUG-42503)
    //to do
    //ui->labelTransactionsStatus->setIcon(icon);
    ui->labelWalletStatus->setIcon(icon);

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);
      
       
    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));

    // init "out of sync" warning labels
    ui->labelWalletStatus->setText( tr("Please wait until the wallet is fully synced to see your correct balance"));
    // to do
    //ui->labelTransactionsStatus->setText(tr("Out of Sync!"));


    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);
    connect(ui->labelWalletStatus, SIGNAL(clicked()), this, SLOT(handleOutOfSyncWarningClicks()));
    // to do
    //connect(ui->labelTransactionsStatus, SIGNAL(clicked()), this, SLOT(handleOutOfSyncWarningClicks()));

}

void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
    if(filter)
        Q_EMIT transactionClicked(filter->mapToSource(index));
}

void OverviewPage::handleOutOfSyncWarningClicks()
{
    Q_EMIT outOfSyncWarningClicked();
}

OverviewPage::~OverviewPage()
{
    // ++ Explorer Stats
    if(m_timer) disconnect(m_timer, SIGNAL(timeout()), this, SLOT(privateSendStatus()));
    delete m_networkManager;
    // --

    delete ui;
}


// ++ Explorer Stats
void OverviewPage::getStatistics()
{
    QUrl summaryUrl("https://explorer.sinovate.io/ext/summary");
    QNetworkRequest request;
    request.setUrl(summaryUrl);
    m_networkManager->get(request);
}
// --

void OverviewPage::setBalance(const interfaces::WalletBalances& balances)
{
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    m_balances = balances;
    totalBalance = balances.balance + balances.unconfirmed_balance + balances.immature_balance;
    ui->labelBalance->setText(BitcoinUnits::floorHtmlWithUnit(unit, balances.balance, false, BitcoinUnits::separatorAlways));
    ui->labelUnconfirmed->setText(BitcoinUnits::floorHtmlWithUnit(unit, balances.unconfirmed_balance, false, BitcoinUnits::separatorAlways));
    ui->labelImmature->setText(BitcoinUnits::floorHtmlWithUnit(unit, balances.immature_balance, false, BitcoinUnits::separatorAlways));
    //to do
    //ui->labelTotal->setText(BitcoinUnits::floorHtmlWithUnit(unit, balances.balance + balances.unconfirmed_balance + balances.immature_balance, false, BitcoinUnits::separatorAlways));
    ui->labelWatchAvailable->setText(BitcoinUnits::floorHtmlWithUnit(unit, balances.watch_only_balance, false, BitcoinUnits::separatorAlways));
    ui->labelWatchPending->setText(BitcoinUnits::floorHtmlWithUnit(unit, balances.unconfirmed_watch_only_balance, false, BitcoinUnits::separatorAlways));
    ui->labelWatchImmature->setText(BitcoinUnits::floorHtmlWithUnit(unit, balances.immature_watch_only_balance, false, BitcoinUnits::separatorAlways));
    ui->labelWatchTotal->setText(BitcoinUnits::floorHtmlWithUnit(unit, balances.watch_only_balance + balances.unconfirmed_watch_only_balance + balances.immature_watch_only_balance, false, BitcoinUnits::separatorAlways));

     // Create the timer
        connect(pricingTimer, SIGNAL(timeout()), this, SLOT(getPriceInfo()));
        pricingTimer->start(300000);
        getPriceInfo();
        /** pricing USD END */

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showImmature = balances.immature_balance != 0;
    bool showWatchOnlyImmature = balances.immature_watch_only_balance != 0;

    // for symmetry reasons also show immature label when the watch-only one is shown
    //ui->labelImmature->setVisible(showImmature || showWatchOnlyImmature);
    //ui->labelImmatureText->setVisible(showImmature || showWatchOnlyImmature);
    // For visual reasons, the Immature label will be constantly shown.
    ui->labelImmature;
    ui->labelImmatureText;
    //
    ui->labelWatchImmature->setVisible(showWatchOnlyImmature); // show watch-only immature balance

    static int cachedTxLocks = 0;

    if(cachedTxLocks != nCompleteTXLocks){
        cachedTxLocks = nCompleteTXLocks;
        ui->listTransactions->update();
    }
}

// show/hide watch-only labels
void OverviewPage::updateWatchOnlyLabels(bool showWatchOnly)
{
    //ui->labelSpendable->setVisible(showWatchOnly);      // show spendable label (only when watch-only is active)
    ui->labelWatchonly->setVisible(showWatchOnly);      // show watch-only label
    //ui->lineWatchBalance->setVisible(showWatchOnly);    // show watch-only balance separator line
    ui->labelWatchAvailable->setVisible(showWatchOnly); // show watch-only available balance
    ui->labelWatchPending->setVisible(showWatchOnly);   // show watch-only pending balance
    ui->labelWatchTotal->setVisible(showWatchOnly);     // show watch-only total balance
    ui->watchOnlyFrame->setVisible(showWatchOnly);
    
    if (!showWatchOnly){
        ui->labelWatchImmature->hide();

    }
    else{
        ui->labelBalance->setIndent(20);
        ui->labelUnconfirmed->setIndent(20);
        ui->labelImmature->setIndent(20);
        //to do
        //ui->labelTotal->setIndent(20);
    }
}

void OverviewPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model)
    {
        // Show warning if this is a prerelease version
        connect(model, SIGNAL(alertsChanged(QString)), this, SLOT(updateAlerts(QString)));
        updateAlerts(model->getStatusBarWarnings());
    }
}

void OverviewPage::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if(model && model->getOptionsModel())
    {
        // Set up transaction list
        filter.reset(new TransactionFilterProxy());
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter.get());
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        interfaces::Wallet& wallet = model->wallet();
        interfaces::WalletBalances balances = wallet.getBalances();
        setBalance(balances);
        connect(model, SIGNAL(balanceChanged(interfaces::WalletBalances)), this, SLOT(setBalance(interfaces::WalletBalances)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
        updateWatchOnlyLabels(wallet.haveWatchOnly());
        connect(model, SIGNAL(notifyWatchonlyChanged(bool)), this, SLOT(updateWatchOnlyLabels(bool)));

        // that's it for litemode
        if(fLiteMode) return;
    }
    // update the display unit, to not use the default ("SIN")
    updateDisplayUnit();
}

void OverviewPage::updateDisplayUnit()
{
    if(walletModel && walletModel->getOptionsModel())
    {
        if (m_balances.balance != -1) {
            setBalance(m_balances);
        }

        // Update txdelegate->unit with the current unit
        txdelegate->unit = walletModel->getOptionsModel()->getDisplayUnit();

        ui->listTransactions->update();
    }
}

void OverviewPage::updateAlerts(const QString &warnings)
{
    this->ui->labelAlerts->setVisible(!warnings.isEmpty());
    this->ui->labelAlerts->setText(warnings);
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->labelWalletStatus->setVisible(fShow);
    ui->layoutWarning->setVisible(fShow);
    // to do
    //ui->labelTransactionsStatus->setVisible(fShow);
}

void OverviewPage::SetupTransactionList(int nNumItems) {
    ui->listTransactions->setMinimumHeight(nNumItems * (DECORATION_SIZE + 2));

    if(walletModel && walletModel->getOptionsModel()) {
        // Set up transaction list
        filter.reset(new TransactionFilterProxy());
        filter->setSourceModel(walletModel->getTransactionTableModel());
        filter->setLimit(nNumItems);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter.get());
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);
    }
}

void OverviewPage::getPriceInfo()
{
        request->setUrl(QUrl("https://stats.sinovate.io/priceUSD.php"));
    
    networkManager->get(*request);
}

// ++ Explorer Stats
void OverviewPage::onResult(QNetworkReply* replystats)
{
    QVariant statusCode = replystats->attribute(QNetworkRequest::HttpStatusCodeAttribute);

    if( statusCode == 200)
    {
        QString replyString = (QString) replystats->readAll();

        QJsonDocument jsonResponse = QJsonDocument::fromJson(replyString.toUtf8());
        QJsonObject jsonObject = jsonResponse.object();

        QJsonObject dataObject = jsonObject.value("data").toArray()[0].toObject();

        QLocale l = QLocale(QLocale::English);

        // Set LatestBTC strings
        ui->labelCurrentPriceBTC->setText(QString::number(dataObject.value("lastPrice").toDouble(), 'f', 8));           

        // Set INFINITY NODE STATS strings
        int bigRoiDays = 1000000/((720/dataObject.value("inf_online_big").toDouble())*1752);
        int midRoiDays = 500000/((720/dataObject.value("inf_online_mid").toDouble())*838);
        int lilRoiDays = 100000/((720/dataObject.value("inf_online_lil").toDouble())*160);

        QString bigROIString = "ROI: " + QString::number(bigRoiDays) + " days" ;
        QString midROIString = "ROI: " + QString::number(midRoiDays) + " days";
        QString lilROIString = "ROI: " + QString::number(lilRoiDays) + " days";
        QString totalNodesString = QString::number(dataObject.value("inf_online_big").toInt() + dataObject.value("inf_online_mid").toInt() + dataObject.value("inf_online_lil").toInt()) + " nodes";
    
        QString bigString = dataObject.value("inf_burnt_big").toVariant().toString() + "/" + dataObject.value("inf_online_big").toVariant().toString() + "/" + bigROIString;
        QString midString = dataObject.value("inf_burnt_mid").toVariant().toString() + "/" + dataObject.value("inf_online_mid").toVariant().toString() + "/" + midROIString;
        QString lilString = dataObject.value("inf_burnt_lil").toVariant().toString() + "/" + dataObject.value("inf_online_lil").toVariant().toString() + "/"  + lilROIString;
        
        // Set supply string
        int supplyNumber = dataObject.value("supply").toDouble() - dataObject.value("burnFee").toDouble();

        ui->totalValueLabel->setText(totalNodesString);
        ui->totalSupplyValueLabel->setText(l.toString(supplyNumber) + " SIN");
        ui->addressesValueLabel->setText(dataObject.value("explorerAddresses").toVariant().toString());
        ui->bigValueLabel->setText(bigString);
        ui->midValueLabel->setText(midString);
        ui->lilValueLabel->setText(lilString);
    }
    else
    {
        const QString noValue = "NaN";
       
        ui->totalValueLabel->setText(noValue);
        ui->totalSupplyValueLabel->setText(noValue);
    }
    replystats->deleteLater();
}
// --