// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/singui.h>

#include <clientversion.h>
#include <qt/sinunits.h>
#include <qt/clientmodel.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/modaloverlay.h>
#include <qt/networkstyle.h>
#include <qt/notificator.h>
#include <qt/openuridialog.h>
#include <qt/optionsdialog.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/rpcconsole.h>
#include <qt/utilitydialog.h>

#ifdef ENABLE_WALLET
#include <qt/walletframe.h>
#include <qt/walletmodel.h>
#include <qt/walletview.h>
#endif // ENABLE_WALLET

#ifdef Q_OS_MAC
#include <qt/macdockiconhandler.h>
#endif

#include <chainparams.h>
#include <interfaces/handler.h>
#include <interfaces/node.h>
#include <ui_interface.h>
#include <util.h>

// Dash
#include <masternode-sync.h>
#include <qt/masternodelist.h>
//

// Instaswap
#include <qt/instaswap.h>
//


// StatsPage
#include <qt/statspage.h>
//

// FaqPage
#include <qt/faqpage.h>
//

#include <iostream>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopWidget>
#include <QDragEnterEvent>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressDialog>
#include <QProgressBar>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QFontDatabase>
#include <QPushButton>
#include <QToolTip>


#if QT_VERSION < 0x050000
#include <QTextDocument>
#include <QUrl>
#else
#include <QUrlQuery>
#endif

const std::string SINGUI::DEFAULT_UIPLATFORM =
#if defined(Q_OS_MAC)
        "macosx"
#elif defined(Q_OS_WIN)
        "windows"
#else
        "other"
#endif
        ;

SINGUI::SINGUI(interfaces::Node& node, const PlatformStyle *_platformStyle, const NetworkStyle *networkStyle, QWidget *parent) :
    QMainWindow(parent),
    m_node(node),
    platformStyle(_platformStyle)
{
	QSettings settings;
    if (!restoreGeometry(settings.value("MainWindowGeometry").toByteArray())) {
        // Restore failed (perhaps missing setting), center the window
        move(QApplication::desktop()->availableGeometry().center() - frameGeometry().center());
    }

       QString windowTitle = tr("SIN-CORE") + " - ";
#ifdef ENABLE_WALLET
    enableWallet = WalletModel::isWalletEnabled();
#endif // ENABLE_WALLET
    if(enableWallet)
    {
        windowTitle += tr("Wallet");
    } else {
        windowTitle += tr("Node");
    }
    windowTitle += " " + networkStyle->getTitleAddText();
    QApplication::setWindowIcon(networkStyle->getTrayAndWindowIcon());
    setWindowIcon(networkStyle->getTrayAndWindowIcon());
    setWindowTitle(windowTitle);

    rpcConsole = new RPCConsole(node, _platformStyle, 0);
    helpMessageDialog = new HelpMessageDialog(node, this, HelpMessageDialog::cmdline);
#ifdef ENABLE_WALLET
    if(enableWallet)
    {
        /** Create wallet frame and make it the central widget */
        walletFrame = new WalletFrame(_platformStyle, this);
        setCentralWidget(walletFrame);
    } else
#endif // ENABLE_WALLET
    {
        /* When compiled without wallet or -disablewallet is provided,
         * the central widget is the rpc console.
         */
        setCentralWidget(rpcConsole);
    }

    QFontDatabase::addApplicationFont(":/fonts/Hind-Bold");
    QFontDatabase::addApplicationFont(":/fonts/Hind-SemiBold");
    QFontDatabase::addApplicationFont(":/fonts/Hind-Regular");
    QFontDatabase::addApplicationFont(":/fonts/Hind-Light");
    QFontDatabase::addApplicationFont(":/fonts/Montserrat-Bold");
    


 // Accept D&D of URIs
    setAcceptDrops(true);

    // Create actions for the toolbar, menu bar and tray/dock icon
    // Needs walletFrame to be initialized
    createActions();

    // Create application menu bar
    createMenuBar();

    // Create the toolbars
    createToolBars();

    // Create system tray icon and notification
    createTrayIcon(networkStyle);

    // Create status bar
    statusBar();

    //// Set widget topButton on the top right corner ////
    QWidget *topButtonWidget = new QWidget;
    QPushButton *topAddressButton = new QPushButton();
    QHBoxLayout *topButtonLayout = new QHBoxLayout;
    topButtonLayout->setContentsMargins(3,0,3,0);
    topButtonLayout->setSpacing(3);
    topButtonLayout->addWidget(topAddressButton);
    topAddressButton->setIcon(QIcon(":/icons/address-book"));
    topAddressButton->setIconSize(QSize(16, 16));
    topAddressButton->setToolTip( "Open Address Book"  );
    topAddressButton->setStyleSheet("QToolTip { color: #000000; background-color: #ffffff; border: 0px; } QPushButton {background-color: transparent; font-size:10px; font-weight:normal; border:none}");
    topButtonWidget->setLayout(topButtonLayout);
    appMenuBar->setCornerWidget(topButtonWidget, Qt::TopRightCorner);
    connect(topAddressButton, SIGNAL(released()), walletFrame, SLOT(usedReceivingAddresses()));
    //// topButtonWidget end ////

   // Disable size grip because it looks ugly and nobody needs it
    statusBar()->setSizeGripEnabled(false);

    // Status bar notification icons
    QFrame *frameBlocks = new QFrame();
    frameBlocks->setContentsMargins(0,0,0,0);
    frameBlocks->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    QHBoxLayout *frameBlocksLayout = new QHBoxLayout(frameBlocks);
    frameBlocksLayout->setContentsMargins(3,0,3,0);
    frameBlocksLayout->setSpacing(3);
    unitDisplayControl = new UnitDisplayStatusBarControl(platformStyle);
    labelWalletEncryptionIcon = new QLabel();
    labelWalletHDStatusIcon = new QLabel();
    labelProxyIcon = new QLabel();
    connectionsControl = new GUIUtil::ClickableLabel();
    labelBlocksIcon = new GUIUtil::ClickableLabel();
    if(enableWallet)
    {
        frameBlocksLayout->addStretch();
        frameBlocksLayout->addWidget(unitDisplayControl);
        frameBlocksLayout->addStretch();
        frameBlocksLayout->addWidget(labelWalletEncryptionIcon);
        frameBlocksLayout->addWidget(labelWalletHDStatusIcon);
    }
    frameBlocksLayout->addWidget(labelProxyIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(connectionsControl);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelBlocksIcon);
    frameBlocksLayout->addStretch();

    // Progress bar and label for blocks download
    progressBarLabel = new QLabel();
    progressBarLabel->setVisible(false);
    progressBar = new GUIUtil::ProgressBar();
    progressBar->setAlignment(Qt::AlignCenter);
    // Dash
    //progressBar->setVisible(false);
    progressBar->setVisible(true);
    //

    // Override style sheet for progress bar for styles that have a segmented progress bar,
    // as they make the text unreadable (workaround for issue #1071)
    // See https://doc.qt.io/qt-5/gallery.html
    QString curStyle = QApplication::style()->metaObject()->className();
    if(curStyle == "QWindowsStyle" || curStyle == "QWindowsXPStyle")
    {
        progressBar->setStyleSheet("QProgressBar { background-color: #e8e8e8; border: 1px solid grey; border-radius: 7px; padding: 1px; text-align: center; } QProgressBar::chunk { background: QLinearGradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #FF8000, stop: 1 orange); border-radius: 7px; margin: 0px; }");
    }

    statusBar()->addWidget(progressBarLabel);
    progressBarLabel->setStyleSheet("QToolTip { color: #000000; background-color: #ffffff; border: 1px solid white; } QLabel {color: #fff; }");
    statusBar()->addWidget(progressBar);
    statusBar()->addPermanentWidget(frameBlocks);

    // Install event filter to be able to catch status tip events (QEvent::StatusTip)
    this->installEventFilter(this);

    // Initially wallet actions should be disabled
    setWalletActionsEnabled(false);

    // Subscribe to notifications from core
    subscribeToCoreSignals();

    connect(connectionsControl, SIGNAL(clicked(QPoint)), this, SLOT(toggleNetworkActive()));

    ///Resorces Web Links
    connect(ResourcesWebsite1, SIGNAL(triggered()), rpcConsole, SLOT(hyperlinks3_slot1()));
    connect(ResourcesWebsite2, SIGNAL(triggered()), rpcConsole, SLOT(hyperlinks3_slot2()));
    connect(ResourcesWebsite3, SIGNAL(triggered()), rpcConsole, SLOT(hyperlinks3_slot3()));
    connect(ResourcesWebsite4, SIGNAL(triggered()), rpcConsole, SLOT(hyperlinks3_slot4()));
    connect(ResourcesWebsite5, SIGNAL(triggered()), rpcConsole, SLOT(hyperlinks3_slot5()));
    connect(ResourcesWebsite6, SIGNAL(triggered()), rpcConsole, SLOT(hyperlinks3_slot6()));
    connect(ResourcesWebsite7, SIGNAL(triggered()), rpcConsole, SLOT(hyperlinks3_slot7()));
    connect(ResourcesWebsite9, SIGNAL(triggered()), rpcConsole, SLOT(hyperlinks3_slot9()));
    connect(ResourcesWebsite10, SIGNAL(triggered()), rpcConsole, SLOT(hyperlinks3_slot10()));
    connect(ResourcesWebsite11, SIGNAL(triggered()), rpcConsole, SLOT(hyperlinks3_slot11()));
    ///end Exhange, Resources and Web Links

    modalOverlay = new ModalOverlay(this->centralWidget());
#ifdef ENABLE_WALLET
    if(enableWallet) {
        connect(walletFrame, SIGNAL(requestedSyncWarningInfo()), this, SLOT(showModalOverlay()));
        connect(labelBlocksIcon, SIGNAL(clicked(QPoint)), this, SLOT(showModalOverlay()));
        connect(progressBar, SIGNAL(clicked(QPoint)), this, SLOT(showModalOverlay()));
    }
#endif
}

SINGUI::~SINGUI()
{
    // Unsubscribe from notifications from core
    unsubscribeFromCoreSignals();

    QSettings settings;
    settings.setValue("MainWindowGeometry", saveGeometry());
    if(trayIcon) // Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
        trayIcon->hide();
#ifdef Q_OS_MAC
    delete appMenuBar;
    MacDockIconHandler::cleanup();
#endif

    delete rpcConsole;
}

void SINGUI::createActions()
{
    QActionGroup *tabGroup = new QActionGroup(this);

    overviewAction = new QAction(platformStyle->SingleColorIcon(":/icons/mywallet"), tr(" &My Wallet\n"), this);
    overviewAction->setStatusTip(tr("Show general overview of wallet"));
    overviewAction->setToolTip(overviewAction->statusTip());
    overviewAction->setCheckable(true);
    overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));
    tabGroup->addAction(overviewAction);
    
    homeAction =new QAction(overviewAction);
    homeAction->setText("Home");
    homeAction->setIcon(QIcon(":/icons/overview1"));
    homeAction->setToolTip(homeAction->statusTip());
    homeAction->setCheckable(true);
    tabGroup->addAction(homeAction);

    sendCoinsAction = new QAction(platformStyle->SingleColorIcon(":/icons/send1"), tr(" &Send\n"), this);
    sendCoinsAction->setStatusTip(tr("Send coins to a SIN address"));
    sendCoinsAction->setToolTip(sendCoinsAction->statusTip());
    sendCoinsAction->setCheckable(true);
    sendCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));
    tabGroup->addAction(sendCoinsAction);

    sendCoinsMenuAction = new QAction(platformStyle->TextColorIcon(":/icons/send"), sendCoinsAction->text(), this);
    sendCoinsMenuAction->setStatusTip(sendCoinsAction->statusTip());
    sendCoinsMenuAction->setToolTip(sendCoinsMenuAction->statusTip());

    depositCoinsAction = new QAction(platformStyle->SingleColorIcon(":/icons/deposit"), tr(" &Earn\n"), this);
    depositCoinsAction->setStatusTip(tr("Timelock coins to a SIN address"));
    depositCoinsAction->setToolTip(depositCoinsAction->statusTip());
    depositCoinsAction->setCheckable(true);
    tabGroup->addAction(depositCoinsAction);

    depositCoinsMenuAction = new QAction(platformStyle->TextColorIcon(":/icons/deposit"), depositCoinsAction->text(), this);
    depositCoinsMenuAction->setStatusTip(depositCoinsAction->statusTip());
    depositCoinsMenuAction->setToolTip(depositCoinsMenuAction->statusTip());

    receiveCoinsAction = new QAction(platformStyle->SingleColorIcon(":/icons/receiving_addresses1"), tr(" &Receive\n"), this);
    receiveCoinsAction->setStatusTip(tr("Request payments (generates QR codes and sin: URIs)"));
    receiveCoinsAction->setToolTip(receiveCoinsAction->statusTip());
    receiveCoinsAction->setCheckable(true);
    receiveCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));
    tabGroup->addAction(receiveCoinsAction);

    receiveCoinsMenuAction = new QAction(platformStyle->TextColorIcon(":/icons/receiving_addresses"), receiveCoinsAction->text(), this);
    receiveCoinsMenuAction->setStatusTip(receiveCoinsAction->statusTip());
    receiveCoinsMenuAction->setToolTip(receiveCoinsMenuAction->statusTip());

    historyAction = new QAction(platformStyle->SingleColorIcon(":/icons/history1"), tr(" &Tx History\n"), this);
    historyAction->setStatusTip(tr("Browse transaction history"));
    historyAction->setToolTip(historyAction->statusTip());
    historyAction->setCheckable(true);
    historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_4));
    tabGroup->addAction(historyAction);


#ifdef ENABLE_WALLET
    // Dash
    QSettings settings;
    if (settings.value("fShowMasternodesTab").toBool()) {
        masternodeAction = new QAction(platformStyle->SingleColorIcon(":/icons/masternodes"), tr(" &Infinity Nodes"), this);
        masternodeAction->setStatusTip(tr("Browse Infinitynodes"));
        masternodeAction->setToolTip(masternodeAction->statusTip());
        masternodeAction->setCheckable(true);
#ifdef Q_OS_MAC
        masternodeAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_5));
#else
        masternodeAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_5));
#endif
        tabGroup->addAction(masternodeAction);
        connect(masternodeAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
        connect(masternodeAction, SIGNAL(triggered()), this, SLOT(gotoMasternodePage()));
    }
    //

    // Instaswap
    instaswapAction = new QAction(platformStyle->SingleColorIcon(":/icons/instaswap1"), tr("&Buy SIN"), this);
    instaswapAction->setStatusTip(tr("Exchange your SIN rapidly"));
    instaswapAction->setToolTip(instaswapAction->statusTip());
    instaswapAction->setCheckable(true);
#ifdef Q_OS_MAC
    instaswapAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_6));
#else
    instaswapAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_6));
#endif
    tabGroup->addAction(instaswapAction);
    connect(instaswapAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(instaswapAction, SIGNAL(triggered()), this, SLOT(gotoInstaswapPage()));

    //
    // StatsPage
    statsPageAction = new QAction(platformStyle->SingleColorIcon(":/icons/stats"), tr(" &Stats\n"), this);
    statsPageAction->setStatusTip(tr("Statistics"));
    statsPageAction->setToolTip(statsPageAction->statusTip());
    statsPageAction->setCheckable(true);

#ifdef Q_OS_MAC
    statsPageAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_7));
#else
    statsPageAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_7));
#endif
    tabGroup->addAction(statsPageAction);
    connect(statsPageAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(statsPageAction, SIGNAL(triggered()), this, SLOT(gotoStatsPage()));
    //

    // FaqPage
    faqPageAction = new QAction(platformStyle->SingleColorIcon(":/icons/faq"), tr("&FAQ\n"), this);
    faqPageAction->setStatusTip(tr("FAQ"));
    faqPageAction->setToolTip(faqPageAction->statusTip());
    faqPageAction->setCheckable(true);

#ifdef Q_OS_MAC
    faqPageAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_8));
#else
    faqPageAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_8));
#endif
    tabGroup->addAction(faqPageAction);
    connect(faqPageAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(faqPageAction, SIGNAL(triggered()), this, SLOT(gotoFaqPage()));
    //

    // These showNormalIfMinimized are needed because Send Coins and Receive Coins
    // can be triggered from the tray menu, and need to show the GUI to be useful.
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(gotoOverviewPage()));
    connect(homeAction, SIGNAL(triggered()), this, SLOT(gotoHomePage()));
    
    connect(sendCoinsAction, &QAction::triggered, [this]{ showNormalIfMinimized(); });
    connect(sendCoinsAction, &QAction::triggered, [this]{ gotoSendCoinsPage(); });
    connect(sendCoinsMenuAction, &QAction::triggered, [this]{ showNormalIfMinimized(); });
    connect(sendCoinsMenuAction, &QAction::triggered, [this]{ gotoSendCoinsPage(); });

    connect(depositCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(depositCoinsAction, SIGNAL(triggered()), this, SLOT(gotoDepositCoinsPage()));
    connect(depositCoinsMenuAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(depositCoinsMenuAction, SIGNAL(triggered()), this, SLOT(gotoDepositCoinsPage()));
    
    connect(receiveCoinsAction, &QAction::triggered, [this]{ showNormalIfMinimized(); });
    connect(receiveCoinsAction, &QAction::triggered, this, &SINGUI::gotoReceiveCoinsPage);
    connect(receiveCoinsMenuAction, &QAction::triggered, [this]{ showNormalIfMinimized(); });
    connect(receiveCoinsMenuAction, &QAction::triggered, this, &SINGUI::gotoReceiveCoinsPage);
    
    connect(historyAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(gotoHistoryPage()));

#endif // ENABLE_WALLET

    quitAction = new QAction(platformStyle->SingleColorIcon(":/icons/exit"), tr("E&xit"), this);
    quitAction->setStatusTip(tr("Quit application"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);
    aboutAction = new QAction(platformStyle->SingleColorIcon(":/icons/sin"), tr("&About %1").arg(tr("SINOVATE")), this);
    aboutAction->setStatusTip(tr("Show information about %1").arg(tr("SINOVATE")));
    aboutAction->setMenuRole(QAction::AboutRole);
    aboutAction->setEnabled(false);
    aboutQtAction = new QAction(platformStyle->SingleColorIcon(":/icons/about_qt"), tr("About &Qt"), this);
    aboutQtAction->setStatusTip(tr("Show information about Qt"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
    optionsAction = new QAction(platformStyle->SingleColorIcon(":/icons/options"), tr("&Options..."), this);
    optionsAction->setStatusTip(tr("Modify configuration options for %1").arg(tr("SINOVATE")));
    optionsAction->setMenuRole(QAction::PreferencesRole);
    optionsAction->setEnabled(false);
    toggleHideAction = new QAction(platformStyle->SingleColorIcon(":/icons/about2"), tr("&Show / Hide"), this);
    toggleHideAction->setStatusTip(tr("Show or hide the main Window"));

    encryptWalletAction = new QAction(platformStyle->SingleColorIcon(":/icons/lock_closed"), tr("&Encrypt Wallet..."), this);
    encryptWalletAction->setStatusTip(tr("Encrypt the private keys that belong to your wallet"));
    encryptWalletAction->setCheckable(true);
    backupWalletAction = new QAction(platformStyle->SingleColorIcon(":/icons/filesave"), tr("&Backup Wallet..."), this);
    backupWalletAction->setStatusTip(tr("Backup wallet to another location"));
    changePassphraseAction = new QAction(platformStyle->SingleColorIcon(":/icons/password"), tr("&Change Passphrase..."), this);
    changePassphraseAction->setStatusTip(tr("Change the passphrase used for wallet encryption"));

    // Dash
    // 0: menu items
    //-//unlockWalletAction = new QAction(tr("&Unlock Wallet..."), this);
    //-//unlockWalletAction->setToolTip(tr("Unlock wallet"));
    //-//lockWalletAction = new QAction(tr("&Lock Wallet"), this);
    //

    signMessageAction = new QAction(platformStyle->SingleColorIcon(":/icons/edit"), tr("Sign &message..."), this);
    signMessageAction->setStatusTip(tr("Sign messages with your SIN addresses to prove you own them"));
    verifyMessageAction = new QAction(platformStyle->SingleColorIcon(":/icons/verify"), tr("&Verify message..."), this);
    verifyMessageAction->setStatusTip(tr("Verify messages to ensure they were signed with specified SIN addresses"));

    // Dash
    //-//openInfoAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation), tr("&Information"), this);
    //-//openInfoAction->setStatusTip(tr("Show diagnostic information"));
    //-//openRPCConsoleAction = new QAction(QIcon(":/icons/" + theme + "/debugwindow"), tr("&Debug console"), this);
    //-//openRPCConsoleAction->setStatusTip(tr("Open debugging console"));
    //-//openGraphAction = new QAction(QIcon(":/icons/" + theme + "/connect_4"), tr("&Network Monitor"), this);
    //-//openGraphAction->setStatusTip(tr("Show network monitor"));
    //-//openPeersAction = new QAction(QIcon(":/icons/" + theme + "/connect_4"), tr("&Peers list"), this);
    //-//openPeersAction->setStatusTip(tr("Show peers info"));
    //-//openRepairAction = new QAction(QIcon(":/icons/" + theme + "/options"), tr("Wallet &Repair"), this);
    //-//openRepairAction->setStatusTip(tr("Show wallet repair options"));
    openConfEditorAction = new QAction(QIcon(":/icons/edit"), tr("Open Wallet &Configuration File (sin.conf)"), this);
    openConfEditorAction->setStatusTip(tr("Open configuration file (sin.conf)"));
    //-//openMNConfEditorAction = new QAction(platformStyle->SingleColorIcon(":/icons/edit"), tr("Open &Infinitynode Configuration File"), this);
    //-//openMNConfEditorAction->setStatusTip(tr("Open InfinityNode configuration file"));
    //-//showBackupsAction = new QAction(QIcon(":/icons/" + theme + "/browse"), tr("Show Automatic &Backups"), this);
    //-//showBackupsAction->setStatusTip(tr("Show automatically created wallet backups"));
    // initially disable the debug window menu items
    //-//openInfoAction->setEnabled(false);
    //-// ## duplicate ## openRPCConsoleAction->setEnabled(false);
    //-//openGraphAction->setEnabled(false);
    //-//openPeersAction->setEnabled(false);
    //-//openRepairAction->setEnabled(false);

    //

    openRPCConsoleAction = new QAction(platformStyle->SingleColorIcon(":/icons/debugwindow"), tr("&Debug window (Console)"), this);
    openRPCConsoleAction->setStatusTip(tr("Open debugging and diagnostic console"));
    // initially disable the debug window menu item
    openRPCConsoleAction->setEnabled(false);

    usedSendingAddressesAction = new QAction(platformStyle->SingleColorIcon(":/icons/address-book"), tr("&Sending addresses..."), this);
    usedSendingAddressesAction->setStatusTip(tr("Show the list of used sending addresses and labels"));
    usedReceivingAddressesAction = new QAction(platformStyle->SingleColorIcon(":/icons/address-book"), tr("&Receiving addresses..."), this);
    usedReceivingAddressesAction->setStatusTip(tr("Show the list of used receiving addresses and labels"));

    openAction = new QAction(platformStyle->SingleColorIcon(":/icons/open"), tr("Open &URI..."), this);
    openAction->setStatusTip(tr("Open a sin: URI or payment request"));

    showHelpMessageAction = new QAction(platformStyle->SingleColorIcon(":/icons/info"), tr("&Command-line options"), this);
    showHelpMessageAction->setMenuRole(QAction::NoRole);
    showHelpMessageAction->setStatusTip(tr("Show the %1 help message to get a list with possible SIN command-line options").arg(tr("SINOVATE")));

        // Dash

    //-//connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    //-//connect(aboutAction, SIGNAL(triggered()), this, SLOT(aboutClicked()));
    //-//connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    //-//connect(optionsAction, SIGNAL(triggered()), this, SLOT(optionsClicked()));
    //-//connect(toggleHideAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));
    //-//connect(showHelpMessageAction, SIGNAL(triggered()), this, SLOT(showHelpMessageClicked()));

    // Jump directly to tabs in RPC-console
    //-//connect(openInfoAction, SIGNAL(triggered()), this, SLOT(showInfo()));
    //-//connect(openRPCConsoleAction, SIGNAL(triggered()), this, SLOT(showConsole()));
    //-//connect(openGraphAction, SIGNAL(triggered()), this, SLOT(showGraph()));
    //-//connect(openPeersAction, SIGNAL(triggered()), this, SLOT(showPeers()));
    //-//connect(openRepairAction, SIGNAL(triggered()), this, SLOT(showRepair()));

    // Open configs and backup folder from menu
    connect(openConfEditorAction, SIGNAL(triggered()), this, SLOT(showConfEditor()));
    //-//connect(openMNConfEditorAction, SIGNAL(triggered()), this, SLOT(showMNConfEditor()));
    //-//connect(showBackupsAction, SIGNAL(triggered()), this, SLOT(showBackups()));

    // Get restart command-line parameters and handle restart
    //-//connect(rpcConsole, SIGNAL(handleRestart(QStringList)), this, SLOT(handleRestart(QStringList)));
    //

    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(aboutClicked()));
    connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    connect(optionsAction, SIGNAL(triggered()), this, SLOT(optionsClicked()));
    connect(toggleHideAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));
    connect(showHelpMessageAction, SIGNAL(triggered()), this, SLOT(showHelpMessageClicked()));
    connect(openRPCConsoleAction, SIGNAL(triggered()), this, SLOT(showDebugWindowActivateConsole()));
    
    // prevents an open debug window from becoming stuck/unusable on client shutdown
    connect(quitAction, SIGNAL(triggered()), rpcConsole, SLOT(hide()));

//start Resources Web Links

    ResourcesWebsite1 = new QAction(QIcon(":/icons/info"), tr("&Whitepaper"), this);
    ResourcesWebsite2 = new QAction(QIcon(":/icons/info"), tr("&Roadmap"), this);
    ResourcesWebsite3 = new QAction(QIcon(":/icons/info"), tr("&Documents"), this);
    ResourcesWebsite4 = new QAction(QIcon(":/icons/info"), tr("&Download sin.conf "), this);
    ResourcesWebsite5 = new QAction(QIcon(":/icons/info"), tr("&Wallets"), this);
    ResourcesWebsite6 = new QAction(QIcon(":/icons/explorer1"), tr("&Explorer"), this);
    ResourcesWebsite7 = new QAction(QIcon(":/icons/info"), tr("&SIN WebTool"), this);
    ResourcesWebsite9 = new QAction(QIcon(":/icons/cmc"), tr("&Exchanges"), this);
    ResourcesWebsite10 = new QAction(QIcon(":/icons/info"), tr("&Social Media Channels"), this);
    ResourcesWebsite11 = new QAction(QIcon(":/icons/info"), tr("&Download Bootstrap"), this);


//end Resources Web Links



#ifdef ENABLE_WALLET
    if(walletFrame)
    {
        connect(encryptWalletAction, SIGNAL(triggered(bool)), walletFrame, SLOT(encryptWallet(bool)));
        connect(backupWalletAction, SIGNAL(triggered()), walletFrame, SLOT(backupWallet()));
        connect(changePassphraseAction, SIGNAL(triggered()), walletFrame, SLOT(changePassphrase()));

        // Dash
        //-//connect(unlockWalletAction, SIGNAL(triggered()), walletFrame, SLOT(unlockWallet()));
        //connect(lockWalletAction, SIGNAL(triggered()), walletFrame, SLOT(lockWallet()));
        //

        connect(signMessageAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
        connect(signMessageAction, SIGNAL(triggered()), this, SLOT(gotoSignMessageTab()));
        connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
        connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(gotoVerifyMessageTab()));
        connect(usedSendingAddressesAction, SIGNAL(triggered()), walletFrame, SLOT(usedSendingAddresses()));
        connect(usedReceivingAddressesAction, SIGNAL(triggered()), walletFrame, SLOT(usedReceivingAddresses()));
        connect(openAction, SIGNAL(triggered()), this, SLOT(openClicked()));
    }
#endif // ENABLE_WALLET

    new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_C), this, SLOT(showDebugWindowActivateConsole()));
    new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_D), this, SLOT(showDebugWindow()));
}


void SINGUI::createMenuBar()
{
#ifdef Q_OS_MAC
    // Create a decoupled menu bar on Mac which stays even if the window is closed
    appMenuBar = new QMenuBar();
#else
    // Get the main window's menu bar on other platforms
    appMenuBar = menuBar();
#endif

    // Configure the menus
    QMenu *file = appMenuBar->addMenu(tr("&File"));
     appMenuBar->addMenu("|");
    if(walletFrame)
    {
        file->addAction(openAction);
        file->addAction(backupWalletAction);
        file->addAction(signMessageAction);
        file->addAction(verifyMessageAction);
        file->addSeparator();
        file->addAction(usedSendingAddressesAction);
        file->addAction(usedReceivingAddressesAction);
        file->addSeparator();
        file->addAction(ResourcesWebsite4); //Download sin.conf
        file->addAction(ResourcesWebsite11); //Download Bootstrap
        file->addSeparator();
    }
    file->addAction(quitAction);

    QMenu *settings = appMenuBar->addMenu(tr("&Settings"));
    if(walletFrame)
    {
        settings->addAction(openRPCConsoleAction);
        settings->addAction(encryptWalletAction);
        settings->addAction(changePassphraseAction);

        // Dash
        //-//settings->addAction(unlockWalletAction);
        //-//settings->addAction(lockWalletAction);
        //

 // Dash
    if(walletFrame)
    {
        QMenu *tools = settings->addMenu(tr("&Tools"));
        //-//tools->addAction(openInfoAction);
        //-//tools->addAction(openRPCConsoleAction);
        //-//tools->addAction(openGraphAction);
        //-//tools->addAction(openPeersAction);
        //-//tools->addAction(openRepairAction);
        //-//tools->addSeparator();
        tools->addAction(openConfEditorAction);
        //--//tools->addAction(openMNConfEditorAction);
        //-//tools->addAction(showBackupsAction);
    }

 //start Resources Links

    if (walletFrame) {
        QMenu* hyperlinks3 = settings->addMenu(tr("&Resources"));
        hyperlinks3->addAction(ResourcesWebsite10);
        hyperlinks3->addAction(ResourcesWebsite9);
        hyperlinks3->addAction(ResourcesWebsite1);
        hyperlinks3->addAction(ResourcesWebsite2);
        hyperlinks3->addAction(ResourcesWebsite3);
        hyperlinks3->addAction(ResourcesWebsite5);
        hyperlinks3->addAction(ResourcesWebsite6);
        hyperlinks3->addAction(ResourcesWebsite7);
                
    }
    //end Resources Links

    //
    // temporarily closed it.
 	//settings->addAction(instaswapAction);
    settings->addSeparator();
    }
    settings->addAction(optionsAction);

    settings->addSeparator();

    QMenu *help = settings->addMenu(tr("&Help"));
    if(walletFrame)
    {
    	help->addAction(openRPCConsoleAction);
    }
    help->addAction(showHelpMessageAction);
    help->addAction(faqPageAction);
    help->addSeparator();
    help->addAction(aboutAction);
    help->addAction(aboutQtAction);

}

void SINGUI::createToolBars()
{
    if(walletFrame)
    {
        QToolBar *toolbar = addToolBar(tr("Tabs toolbar"));
        addToolBar(Qt::LeftToolBarArea, toolbar);
        appToolBar = toolbar;
        toolbar->setContextMenuPolicy(Qt::PreventContextMenu);
        toolbar->setMovable(false);
        toolbar->setOrientation(Qt::Vertical);
        toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        toolbar->setMinimumWidth(160);
        mainIcon = new QLabel (this);
    	mainIcon->setPixmap(QPixmap(":images/nav-logo-sin"));
    	mainIcon->setAlignment(Qt::AlignCenter);
    	mainIcon->show();
    	mainIcon->setStyleSheet("QLabel { margin-top: 10px; margin-bottom: 10px; }");
    	

        QWidget* empty3 = new QWidget();
		empty3->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
		toolbar->addWidget(empty3); //1
        
        mainBrand = new QLabel (this);
    	mainBrand->setText("SINOVATE");
    	mainBrand->setAlignment(Qt::AlignCenter);
    	mainBrand->show();
    	mainBrand->setStyleSheet("QLabel { color:#FFFFFF; font-size:16px; font-weight:bold;}");
    	
        toolbar->addWidget(mainIcon);  //2
        toolbar->addWidget(mainBrand); //3

              
        QWidget* empty2 = new QWidget();
		empty2->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
		toolbar->addWidget(empty2); //4
        toolbar->addAction(homeAction);
        toolbar->addAction(overviewAction);  
        toolbar->addAction(depositCoinsAction);
        
        // Dash
        QSettings settings;
        if (settings.value("fShowMasternodesTab").toBool())
        {
            toolbar->addAction(masternodeAction);
        }

        //InstaSwap
        //toolbar->addAction(instaswapAction);
    	//

        // StatsPage
        toolbar->addAction(statsPageAction);
        //
        toolbar->addAction(historyAction);


        homeAction->setChecked(true);

        QLayout* lay = toolbar->layout();
        for(int i = 4; i < lay->count(); ++i)
            lay->itemAt(i)->setAlignment(Qt::AlignLeft);

#ifdef ENABLE_WALLET
        QWidget *spacer = new QWidget();
        spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        toolbar->addWidget(spacer);

        m_wallet_selector = new QComboBox();
        connect(m_wallet_selector, SIGNAL(currentIndexChanged(int)), this, SLOT(setCurrentWalletBySelectorIndex(int)));

        m_wallet_selector_label = new QLabel();
        m_wallet_selector_label->setText(tr("Wallet:") + " ");
        m_wallet_selector_label->setBuddy(m_wallet_selector);

        m_wallet_selector_label_action = appToolBar->addWidget(m_wallet_selector_label);
        m_wallet_selector_action = appToolBar->addWidget(m_wallet_selector);

        m_wallet_selector_label_action->setVisible(false);
        m_wallet_selector_action->setVisible(false);
#endif

         QWidget* empty = new QWidget();
		empty->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
		toolbar->addWidget(empty);

        QLabel* labelVersionName = new QLabel();
        labelVersionName->setText("DIN");
        labelVersionName->setStyleSheet("margin-top: 10px; color: #00FFFF ; font-size: 16px; font-weight : bold;");
        labelVersionName->setAlignment(Qt::AlignCenter);
        
		QLabel* labelVersion = new QLabel();
        labelVersion->setText(QString(tr("BETELGEUSE\nv%1\n")).arg(QString::fromStdString(FormatVersionFriendly())));
        labelVersion->setStyleSheet("color: white ; margin-bottom: 2px; font-weight : bold;");
        labelVersion->setAlignment(Qt::AlignCenter);
        
         //// Set widget topBar on the bottom left corner ////

        QWidget *topBar = new QWidget;
        topThemeButton = new QPushButton();
        QPushButton *topSetupButton = new QPushButton();
        QPushButton *topConsoleButton = new QPushButton();
        QPushButton *topOptionButton = new QPushButton();
        QPushButton *topAddressButton = new QPushButton();
        QPushButton *topFaqButton = new QPushButton();


        QHBoxLayout *topBarLayout = new QHBoxLayout;

        topBarLayout->addWidget(topThemeButton);
        if (settings.value("fShowMasternodesTab").toBool()) { 
        topBarLayout->addWidget(topSetupButton);
        }
        topBarLayout->addWidget(topConsoleButton);
    
        bool lightTheme = settings.value("lightTheme", false).toBool();

        QString cssFileName = lightTheme ? ":/css/light" : ":/css/default";
        QFile cssFile(cssFileName);

        if (lightTheme) {
        topThemeButton->setIcon(QIcon(":/icons/theme-light"));
        topThemeButton->setToolTip( "White Theme"  );
        } else {
        topThemeButton->setIcon(QIcon(":/icons/theme-white"));
        topThemeButton->setToolTip( "Light Theme"  );
        }

        cssFile.open(QFile::ReadOnly);
        QString styleSheet = QLatin1String(cssFile.readAll());
        this->setStyleSheet(styleSheet);

        topThemeButton->setIconSize(QSize(16, 16));
        topThemeButton->setStyleSheet("QToolTip { color: #000000; background-color: #ffffff; border: 0px; } QPushButton {background-color: transparent;} QPushButton:hover {border: 1px solid  #00FFFF; }");

        topSetupButton->setIcon(QIcon(":/icons/setup_top"));
        topSetupButton->setIconSize(QSize(38, 16));
        topSetupButton->setToolTip( "Open SetUP Wizard"  );
        topSetupButton->setStyleSheet("QToolTip { color: #000000; background-color: #ffffff; border: 0px; } QPushButton {background-color: transparent; } QPushButton:hover {border: 1px solid  #00FFFF; } ");
    
    
        topConsoleButton->setIcon(QIcon(":/icons/debugwindow"));
        topConsoleButton->setIconSize(QSize(16, 16));
        topConsoleButton->setToolTip( "Open Console"  );
        topConsoleButton->setStyleSheet("QToolTip { color: #000000; background-color: #ffffff; border: 0px; } QPushButton {background-color: transparent;} QPushButton:hover {border: 1px solid  #00FFFF; }");

        topBar->setLayout(topBarLayout);
        toolbar->addWidget(topBar);


        connect(topThemeButton, SIGNAL (released()), this, SLOT (onThemeClicked()));
        connect(topSetupButton, SIGNAL(released()), this, SLOT(gotoSetupTab()));
        connect(topConsoleButton, SIGNAL (released()), this, SLOT (showDebugWindowActivateConsole()));
    
        //// topBar end ////
      
        toolbar->addWidget(labelVersion);

    }
}

void SINGUI::setClientModel(ClientModel *_clientModel)
{
    this->clientModel = _clientModel;
    if(_clientModel)
    {
        // Create system tray menu (or setup the dock menu) that late to prevent users from calling actions,
        // while the client has not yet fully loaded
        createTrayIconMenu();

        // Keep up to date with client
        updateNetworkState();
        connect(_clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumConnections(int)));
        connect(_clientModel, SIGNAL(networkActiveChanged(bool)), this, SLOT(setNetworkActive(bool)));

        modalOverlay->setKnownBestHeight(_clientModel->getHeaderTipHeight(), QDateTime::fromTime_t(_clientModel->getHeaderTipTime()));
        setNumBlocks(m_node.getNumBlocks(), QDateTime::fromTime_t(m_node.getLastBlockTime()), m_node.getVerificationProgress(), false);
        connect(_clientModel, SIGNAL(numBlocksChanged(int,QDateTime,double,bool)), this, SLOT(setNumBlocks(int,QDateTime,double,bool)));

        // Receive and report messages from client model
        connect(_clientModel, SIGNAL(message(QString,QString,unsigned int)), this, SLOT(message(QString,QString,unsigned int)));

        // Show progress dialog
        connect(_clientModel, SIGNAL(showProgress(QString,int)), this, SLOT(showProgress(QString,int)));

        rpcConsole->setClientModel(_clientModel);

        updateProxyIcon();

#ifdef ENABLE_WALLET
        if(walletFrame)
        {
            walletFrame->setClientModel(_clientModel);
        }
#endif // ENABLE_WALLET
        unitDisplayControl->setOptionsModel(_clientModel->getOptionsModel());

        OptionsModel* optionsModel = _clientModel->getOptionsModel();
        if(optionsModel)
        {
            // be aware of the tray icon disable state change reported by the OptionsModel object.
            connect(optionsModel,SIGNAL(hideTrayIconChanged(bool)),this,SLOT(setTrayIconVisible(bool)));

            // initialize the disable state of the tray icon with the current value in the model.
            setTrayIconVisible(optionsModel->getHideTrayIcon());
        }
    } else {
        // Disable possibility to show main window via action
        toggleHideAction->setEnabled(false);
        if(trayIconMenu)
        {
            // Disable context menu on tray icon
            trayIconMenu->clear();
        }
        // Propagate cleared model to child objects
        rpcConsole->setClientModel(nullptr);
#ifdef ENABLE_WALLET
        if (walletFrame)
        {
            walletFrame->setClientModel(nullptr);
        }
#endif // ENABLE_WALLET
        unitDisplayControl->setOptionsModel(nullptr);
    }
}

#ifdef ENABLE_WALLET
bool SINGUI::addWallet(WalletModel *walletModel)
{
    if(!walletFrame)
        return false;
    const QString name = walletModel->getWalletName();
    QString display_name = name.isEmpty() ? "["+tr("default wallet")+"]" : name;
    setWalletActionsEnabled(true);
    m_wallet_selector->addItem(display_name, name);
    if (m_wallet_selector->count() == 2) {
        m_wallet_selector_label_action->setVisible(true);
        m_wallet_selector_action->setVisible(true);
    }
    rpcConsole->addWallet(walletModel);
    return walletFrame->addWallet(walletModel);
}

bool SINGUI::removeWallet(WalletModel* walletModel)
{
    if (!walletFrame) return false;
    QString name = walletModel->getWalletName();
    int index = m_wallet_selector->findData(name);
    m_wallet_selector->removeItem(index);
    if (m_wallet_selector->count() == 0) {
        setWalletActionsEnabled(false);
    } else if (m_wallet_selector->count() == 1) {
        m_wallet_selector_label_action->setVisible(false);
        m_wallet_selector_action->setVisible(false);
    }
    rpcConsole->removeWallet(walletModel);
    return walletFrame->removeWallet(name);
}

bool SINGUI::setCurrentWallet(const QString& name)
{
    if(!walletFrame)
        return false;
    return walletFrame->setCurrentWallet(name);
}

bool SINGUI::setCurrentWalletBySelectorIndex(int index)
{
    QString internal_name = m_wallet_selector->itemData(index).toString();
    return setCurrentWallet(internal_name);
}

void SINGUI::removeAllWallets()
{
    if(!walletFrame)
        return;
    setWalletActionsEnabled(false);
    walletFrame->removeAllWallets();
}
#endif // ENABLE_WALLET

void SINGUI::setWalletActionsEnabled(bool enabled)
{
    overviewAction->setEnabled(enabled);
    sendCoinsAction->setEnabled(enabled);
    sendCoinsMenuAction->setEnabled(enabled);
    depositCoinsAction->setEnabled(enabled);
    depositCoinsMenuAction->setEnabled(enabled);
    receiveCoinsAction->setEnabled(enabled);
    receiveCoinsMenuAction->setEnabled(enabled);
    historyAction->setEnabled(enabled);

    // Dash
    QSettings settings;
    if (settings.value("fShowMasternodesTab").toBool() && masternodeAction) {
        masternodeAction->setEnabled(enabled);
    }
    //

    // Instaswap
    instaswapAction->setEnabled(enabled);
	//

    // StatsPage
    statsPageAction->setEnabled(enabled);
    //

    // FaqPage
    faqPageAction->setEnabled(enabled);
    //

    encryptWalletAction->setEnabled(enabled);
    backupWalletAction->setEnabled(enabled);
    changePassphraseAction->setEnabled(enabled);
    signMessageAction->setEnabled(enabled);
    verifyMessageAction->setEnabled(enabled);
    usedSendingAddressesAction->setEnabled(enabled);
    usedReceivingAddressesAction->setEnabled(enabled);
    openAction->setEnabled(enabled);
}

void SINGUI::createTrayIcon(const NetworkStyle *networkStyle)
{
#ifndef Q_OS_MAC
    trayIcon = new QSystemTrayIcon(this);
    QString toolTip = tr("%1 client").arg(tr("SINOVATE")) + " " + networkStyle->getTitleAddText();
    trayIcon->setToolTip(toolTip);
    trayIcon->setIcon(networkStyle->getTrayAndWindowIcon());
    trayIcon->hide();
#endif

    notificator = new Notificator(QApplication::applicationName(), trayIcon, this);
}

void SINGUI::createTrayIconMenu()
{
#ifndef Q_OS_MAC
    // return if trayIcon is unset (only on non-Mac OSes)
    if (!trayIcon)
        return;

    trayIconMenu = new QMenu(this);
    trayIcon->setContextMenu(trayIconMenu);


//start Resources Web Links

    trayIconMenu->addAction(ResourcesWebsite9);
    trayIconMenu->addAction(ResourcesWebsite1);
    trayIconMenu->addAction(ResourcesWebsite2);
    trayIconMenu->addAction(ResourcesWebsite3);
    trayIconMenu->addAction(ResourcesWebsite4);
    trayIconMenu->addAction(ResourcesWebsite5);
    trayIconMenu->addAction(ResourcesWebsite6);
    trayIconMenu->addAction(ResourcesWebsite7);
    trayIconMenu->addAction(statsPageAction);
    trayIconMenu->addAction(faqPageAction);
    trayIconMenu->addAction(ResourcesWebsite11);

//end Exchange and Web Links




    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
#else
    // Note: On Mac, the dock icon is used to provide the tray's functionality.
    MacDockIconHandler *dockIconHandler = MacDockIconHandler::instance();
    connect(dockIconHandler, &MacDockIconHandler::dockIconClicked, this, &SINGUI::macosDockIconActivated);

    trayIconMenu = new QMenu(this);
    trayIconMenu->setAsDockMenu();
#endif

    // Configuration of the tray icon (or Dock icon) menu
#ifndef Q_OS_MAC
    // Note: On macOS, the Dock icon's menu already has Show / Hide action.
    trayIconMenu->addAction(toggleHideAction);
    trayIconMenu->addSeparator();
#endif
    if (enableWallet) {
        trayIconMenu->addAction(sendCoinsMenuAction);
        trayIconMenu->addAction(depositCoinsMenuAction);
        trayIconMenu->addAction(receiveCoinsMenuAction);
        trayIconMenu->addSeparator();
        trayIconMenu->addAction(signMessageAction);
        trayIconMenu->addAction(verifyMessageAction);
        trayIconMenu->addSeparator();
        trayIconMenu->addAction(openRPCConsoleAction);
    }
    trayIconMenu->addAction(optionsAction);
    trayIconMenu->addAction(openRPCConsoleAction);

    // Dash
    //-//trayIconMenu->addAction(openGraphAction);
    //-//trayIconMenu->addAction(openPeersAction);
    //-//trayIconMenu->addAction(openRepairAction);
    //-//trayIconMenu->addSeparator();
    trayIconMenu->addAction(openConfEditorAction);
    //--//trayIconMenu->addAction(openMNConfEditorAction);
    //-//trayIconMenu->addAction(showBackupsAction);
    //

#ifndef Q_OS_MAC // This is built-in on macOS
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
#endif
}

#ifndef Q_OS_MAC
void SINGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(reason == QSystemTrayIcon::Trigger)
    {
        // Click on system tray icon triggers show/hide of the main window
        toggleHidden();
    }
}
#else
void SINGUI::macosDockIconActivated()
{
    show();
    activateWindow();
}
#endif

void SINGUI::optionsClicked()
{
    if(!clientModel || !clientModel->getOptionsModel())
        return;

    OptionsDialog dlg(this, enableWallet);
    dlg.setModel(clientModel->getOptionsModel());
    dlg.exec();
}

void SINGUI::aboutClicked()
{
    if(!clientModel)
        return;

    HelpMessageDialog dlg(m_node, this, HelpMessageDialog::about);
    dlg.exec();
}

void SINGUI::showDebugWindow()
{
    GUIUtil::bringToFront(rpcConsole);
}

void SINGUI::showDebugWindowActivateConsole()
{
    rpcConsole->setTabFocus(RPCConsole::TAB_CONSOLE);
    showDebugWindow();
}

void SINGUI::onThemeClicked()
{
    QSettings settings;
    bool lightTheme = !settings.value("lightTheme", false).toBool();

    QString cssFileName = lightTheme ? ":/css/light" : ":/css/default";
    QFile cssFile(cssFileName);

    // Store theme
    settings.setValue("lightTheme", lightTheme);

    if (lightTheme) {
        topThemeButton->setIcon(QIcon(":/icons/theme-light"));
        topThemeButton->setToolTip( "White Theme"  );
    } else {
        topThemeButton->setIcon(QIcon(":/icons/theme-white"));
        topThemeButton->setToolTip( "Light Theme"  );
    }

    cssFile.open(QFile::ReadOnly);
    QString styleSheet = QLatin1String(cssFile.readAll());
    this->setStyleSheet(styleSheet);
}

// Dash
void SINGUI::showMNConfEditor()
{
    GUIUtil::openMNConfigfile();
}
//

// open conf file
void SINGUI::showConfEditor()
{
    GUIUtil::openSinConf();
}
//

void SINGUI::showHelpMessageClicked()
{
    helpMessageDialog->show();
}


#ifdef ENABLE_WALLET
void SINGUI::openClicked()
{
    OpenURIDialog dlg(this);
    if(dlg.exec())
    {
        Q_EMIT receivedURI(dlg.getURI());
    }
}

void SINGUI::gotoOverviewPage()
{
    overviewAction->setChecked(true);
    
    if (walletFrame) walletFrame->gotoOverviewPage();
}


void SINGUI::gotoHomePage()
{
    homeAction->setChecked(true);
    
    //if (walletFrame) walletFrame->gotoOverviewPage();
    if (walletFrame) walletFrame->gotoHomePage();
}

void SINGUI::gotoHistoryPage()
{
    historyAction->setChecked(true);
    if (walletFrame) walletFrame->gotoHistoryPage();
}

// Dash
void SINGUI::gotoMasternodePage()
{
    QSettings settings;
    if (settings.value("fShowMasternodesTab").toBool()) {
        masternodeAction->setChecked(true);
        if (walletFrame) walletFrame->gotoMasternodePage();
    }
}
//

// Instaswap
void SINGUI::gotoInstaswapPage()
{
    instaswapAction->setChecked(true);
    if (walletFrame) walletFrame->gotoInstaswapPage();
}
//


// Statspage
void SINGUI::gotoStatsPage()
{
         statsPageAction->setChecked(true);
    if (walletFrame) walletFrame->gotoStatsPage();
}
//

// Faqpage
void SINGUI::gotoFaqPage()
{
         faqPageAction->setChecked(true);
    if (walletFrame) walletFrame->gotoFaqPage();
}
//


void SINGUI::gotoReceiveCoinsPage()
{
    receiveCoinsAction->setChecked(true);
    if (walletFrame) walletFrame->gotoReceiveCoinsPage();
}

void SINGUI::gotoSendCoinsPage(QString addr)
{
    sendCoinsAction->setChecked(true);
    if (walletFrame) walletFrame->gotoSendCoinsPage(addr);
}

void SINGUI::gotoDepositCoinsPage(QString addr)
{
    depositCoinsAction->setChecked(true);
    if (walletFrame) walletFrame->gotoDepositCoinsPage(addr);
}

void SINGUI::gotoSignMessageTab(QString addr)
{
    if (walletFrame) walletFrame->gotoSignMessageTab(addr);
}

void SINGUI::gotoVerifyMessageTab(QString addr)
{
    if (walletFrame) walletFrame->gotoVerifyMessageTab();
}

void SINGUI::gotoSetupTab()
{
    if (walletFrame) walletFrame->gotoSetupTab();
    masternodeAction->setChecked(true);
    
}

#endif // ENABLE_WALLET

void SINGUI::updateNetworkState()
{
    int count = clientModel->getNumConnections();
    QString icon;
    switch(count)
    {
    case 0: icon = ":/icons/connect_0"; break;
    case 1: case 2: case 3: icon = ":/icons/connect_1"; break;
    case 4: case 5: case 6: icon = ":/icons/connect_2"; break;
    case 7: case 8: case 9: icon = ":/icons/connect_3"; break;
    default: icon = ":/icons/connect_4"; break;
    }

    QString tooltip;

    if (m_node.getNetworkActive()) {
        tooltip = tr("%n active connection(s) to SIN network", "", count) + QString(".<br>") + tr("Click to disable network activity.");
    } else {
        tooltip = tr("Network activity disabled.") + QString("<br>") + tr("Click to enable network activity again.");
        icon = ":/icons/network_disabled";
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");
    connectionsControl->setToolTip(tooltip);

    connectionsControl->setPixmap(platformStyle->SingleColorIcon(icon).pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
}

void SINGUI::setNumConnections(int count)
{
    updateNetworkState();
}

void SINGUI::setNetworkActive(bool networkActive)
{
    updateNetworkState();
}

void SINGUI::updateHeadersSyncProgressLabel()
{
    int64_t headersTipTime = clientModel->getHeaderTipTime();
    int headersTipHeight = clientModel->getHeaderTipHeight();
    int estHeadersLeft = (GetTime() - headersTipTime) / Params().GetConsensus().nPowTargetSpacing;
    if (estHeadersLeft > HEADER_HEIGHT_DELTA_SYNC)
        progressBarLabel->setText(tr("Syncing Headers (%1%)...").arg(QString::number(100.0 / (headersTipHeight+estHeadersLeft)*headersTipHeight, 'f', 1)));
}

void SINGUI::setNumBlocks(int count, const QDateTime& blockDate, double nVerificationProgress, bool header)
{
    if (modalOverlay)
    {
        if (header)
            modalOverlay->setKnownBestHeight(count, blockDate);
        else
            modalOverlay->tipUpdate(count, blockDate, nVerificationProgress);
    }
    if (!clientModel)
        return;

    // Prevent orphan statusbar messages (e.g. hover Quit in main menu, wait until chain-sync starts -> garbled text)
    statusBar()->clearMessage();

    // Acquire current block source
    enum BlockSource blockSource = clientModel->getBlockSource();
    switch (blockSource) {
        case BlockSource::NETWORK:
            if (header) {
                updateHeadersSyncProgressLabel();
                return;
            }
            progressBarLabel->setText(tr("Synchronizing with network..."));
            updateHeadersSyncProgressLabel();
            break;
        case BlockSource::DISK:
            if (header) {
                progressBarLabel->setText(tr("Indexing blocks on disk..."));
            } else {
                progressBarLabel->setText(tr("Processing blocks on disk..."));
            }
            break;
        case BlockSource::REINDEX:
            progressBarLabel->setText(tr("Reindexing blocks on disk..."));
            break;
        case BlockSource::NONE:
            if (header) {
                return;
            }
            progressBarLabel->setText(tr("Connecting to peers..."));
            break;
    }

    QString tooltip;

    QDateTime currentDate = QDateTime::currentDateTime();
    qint64 secs = blockDate.secsTo(currentDate);

    tooltip = tr("Processed %n block(s) of transaction history.", "", count);

    // Set icon state: spinning if catching up, tick otherwise
    if(secs < 18 * 60) //Allow blocks to not appear for 18 minutes
    {
        tooltip = tr("Up to date") + QString(".<br>") + tooltip;
        progressBarLabel->setVisible(false);
        progressBar->setVisible(false);
        labelBlocksIcon->setPixmap(platformStyle->SingleColorIcon(":/icons/synced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));

#ifdef ENABLE_WALLET
        if(walletFrame)
        {
            walletFrame->showOutOfSyncWarning(false);
            modalOverlay->showHide(true, true);

        }
#endif // ENABLE_WALLET
    } else {
        QString timeBehindText = GUIUtil::formatNiceTimeOffset(secs);

        progressBarLabel->setVisible(true);
        progressBar->setFormat(tr("%1 behind").arg(timeBehindText));
        progressBar->setMaximum(1000000000);
        progressBar->setValue(nVerificationProgress * 1000000000.0 + 0.5);
        progressBar->setVisible(true);

        tooltip = tr("Catching up...") + QString("<br>") + tooltip;
        if(count != prevBlocks)
        {
            labelBlocksIcon->setPixmap(platformStyle->SingleColorIcon(QString(
                ":/movies/spinner-%1").arg(spinnerFrame, 3, 10, QChar('0')))
                .pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
            spinnerFrame = (spinnerFrame + 1) % SPINNER_FRAMES;
        }
        prevBlocks = count;

#ifdef ENABLE_WALLET
        if(walletFrame)
        {
            walletFrame->showOutOfSyncWarning(true);
            modalOverlay->showHide();
        }
#endif // ENABLE_WALLET

        tooltip += QString("<br>");
        tooltip += tr("Last received block was generated %1 ago.").arg(timeBehindText);
        tooltip += QString("<br>");
        tooltip += tr("Transactions after this will not yet be visible.");
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");

    labelBlocksIcon->setToolTip(tooltip);
    progressBarLabel->setToolTip(tooltip);
    progressBar->setToolTip(tooltip);
}

/*
void SINGUI::setAdditionalDataSyncProgress(double nSyncProgress)
{
    if(!clientModel)
        return;

    // No additional data sync should be happening while blockchain is not synced, nothing to update
    if(!masternodeSync.IsBlockchainSynced())
        return;

    // Prevent orphan statusbar messages (e.g. hover Quit in main menu, wait until chain-sync starts -> garbelled text)
    statusBar()->clearMessage();

    QString tooltip;

    // Set icon state: spinning if catching up, tick otherwise
    //QString theme = GUIUtil::getThemeName();

    QString strSyncStatus;
    tooltip = tr("Up to date") + QString(".<br>") + tooltip;

    if(masternodeSync.IsSynced()) {
        progressBarLabel->setVisible(false);
        progressBar->setVisible(false);
        //labelBlocksIcon->setPixmap(QIcon(":/icons/" + theme + "/synced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        labelBlocksIcon->setPixmap(platformStyle->SingleColorIcon(":/icons/synced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        //
    } else {

        labelBlocksIcon->setPixmap(platformStyle->SingleColorIcon(QString(
            ":/movies/spinner-%1").arg(spinnerFrame, 3, 10, QChar('0')))
            .pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        spinnerFrame = (spinnerFrame + 1) % SPINNER_FRAMES;

#ifdef ENABLE_WALLET
        if(walletFrame)
            walletFrame->showOutOfSyncWarning(false);
#endif // ENABLE_WALLET

        progressBar->setFormat(tr("Synchronizing additional data: %p%"));
        progressBar->setMaximum(1000000000);
        progressBar->setValue(nSyncProgress * 1000000000.0 + 0.5);
    }

    strSyncStatus = QString(masternodeSync.GetSyncStatus().c_str());
    progressBarLabel->setText(strSyncStatus);
    tooltip = strSyncStatus + QString("<br>") + tooltip;

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");

    labelBlocksIcon->setToolTip(tooltip);
    progressBarLabel->setToolTip(tooltip);
    progressBar->setToolTip(tooltip);
}*/

void SINGUI::message(const QString &title, const QString &message, unsigned int style, bool *ret)
{
    QString strTitle = tr("SIN"); // default title
    // Default to information icon
    int nMBoxIcon = QMessageBox::Information;
    int nNotifyIcon = Notificator::Information;

    QString msgType;

    // Prefer supplied title over style based title
    if (!title.isEmpty()) {
        msgType = title;
    }
    else {
        switch (style) {
        case CClientUIInterface::MSG_ERROR:
            msgType = tr("Error");
            break;
        case CClientUIInterface::MSG_WARNING:
            msgType = tr("Warning");
            break;
        case CClientUIInterface::MSG_INFORMATION:
            msgType = tr("Information");
            break;
        default:
            break;
        }
    }
    // Append title to "SIN - "
    if (!msgType.isEmpty())
        strTitle += " - " + msgType;

    // Check for error/warning icon
    if (style & CClientUIInterface::ICON_ERROR) {
        nMBoxIcon = QMessageBox::Critical;
        nNotifyIcon = Notificator::Critical;
    }
    else if (style & CClientUIInterface::ICON_WARNING) {
        nMBoxIcon = QMessageBox::Warning;
        nNotifyIcon = Notificator::Warning;
    }

    // Display message
    if (style & CClientUIInterface::MODAL) {
        // Check for buttons, use OK as default, if none was supplied
        QMessageBox::StandardButton buttons;
        if (!(buttons = (QMessageBox::StandardButton)(style & CClientUIInterface::BTN_MASK)))
            buttons = QMessageBox::Ok;

        showNormalIfMinimized();
        QMessageBox mBox(static_cast<QMessageBox::Icon>(nMBoxIcon), strTitle, message, buttons, this);
        mBox.setTextFormat(Qt::PlainText);
        int r = mBox.exec();
        if (ret != nullptr)
            *ret = r == QMessageBox::Ok;
    }
    else
        notificator->notify(static_cast<Notificator::Class>(nNotifyIcon), strTitle, message);
}

void SINGUI::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
#ifndef Q_OS_MAC // Ignored on Mac
    if(e->type() == QEvent::WindowStateChange)
    {
        if(clientModel && clientModel->getOptionsModel() && clientModel->getOptionsModel()->getMinimizeToTray())
        {
            QWindowStateChangeEvent *wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if(!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized())
            {
                QTimer::singleShot(0, this, SLOT(hide()));
                e->ignore();
            }
            else if((wsevt->oldState() & Qt::WindowMinimized) && !isMinimized())
            {
                QTimer::singleShot(0, this, SLOT(show()));
                e->ignore();
            }
        }
    }
#endif
}

void SINGUI::closeEvent(QCloseEvent *event)
{
#ifndef Q_OS_MAC // Ignored on Mac
    if(clientModel && clientModel->getOptionsModel())
    {
        if(!clientModel->getOptionsModel()->getMinimizeOnClose())
        {
            // close rpcConsole in case it was open to make some space for the shutdown window
            rpcConsole->close();

            QApplication::quit();
        }
        else
        {
            QMainWindow::showMinimized();
            event->ignore();
        }
    }
#else
    QMainWindow::closeEvent(event);
#endif
}

void SINGUI::showEvent(QShowEvent *event)
{
    // enable the debug window when the main window shows up
    openRPCConsoleAction->setEnabled(true);
    aboutAction->setEnabled(true);
    optionsAction->setEnabled(true);
}

#ifdef ENABLE_WALLET
void SINGUI::incomingTransaction(const QString& date, int unit, const CAmount& amount, const QString& type, const QString& address, const QString& label, const QString& walletName)
{
    // On new transaction, make an info balloon
    QString msg = tr("Date: %1\n").arg(date) +
                  tr("Amount: %1\n").arg(BitcoinUnits::formatWithUnit(unit, amount, true));
    if (m_node.getWallets().size() > 1 && !walletName.isEmpty()) {
        msg += tr("Wallet: %1\n").arg(walletName);
    }
    msg += tr("Type: %1\n").arg(type);
    if (!label.isEmpty())
        msg += tr("Label: %1\n").arg(label);
    else if (!address.isEmpty())
        msg += tr("Address: %1\n").arg(address);
    message((amount)<0 ? tr("Sent transaction") : tr("Incoming transaction"),
             msg, CClientUIInterface::MSG_INFORMATION);
}
#endif // ENABLE_WALLET

void SINGUI::dragEnterEvent(QDragEnterEvent *event)
{
    // Accept only URIs
    if(event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void SINGUI::dropEvent(QDropEvent *event)
{
    if(event->mimeData()->hasUrls())
    {
        for (const QUrl &uri : event->mimeData()->urls())
        {
            Q_EMIT receivedURI(uri.toString());
        }
    }
    event->acceptProposedAction();
}

bool SINGUI::eventFilter(QObject *object, QEvent *event)
{
    // Catch status tip events
    if (event->type() == QEvent::StatusTip)
    {
        // Prevent adding text from setStatusTip(), if we currently use the status bar for displaying other stuff
        if (progressBarLabel->isVisible() || progressBar->isVisible())
            return true;
    }
    return QMainWindow::eventFilter(object, event);
}

#ifdef ENABLE_WALLET
bool SINGUI::handlePaymentRequest(const SendCoinsRecipient& recipient)
{
    // URI has to be valid
    if (walletFrame && walletFrame->handlePaymentRequest(recipient))
    {
        showNormalIfMinimized();
        gotoSendCoinsPage();
        return true;
    }
    return false;
}

void SINGUI::setHDStatus(int hdEnabled)
{
    labelWalletHDStatusIcon->setPixmap(platformStyle->SingleColorIcon(hdEnabled ? ":/icons/hd_enabled" : ":/icons/hd_disabled").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
    labelWalletHDStatusIcon->setToolTip(hdEnabled ? tr("HD key generation is <b>enabled</b>") : tr("HD key generation is <b>disabled</b>"));

    // eventually disable the QLabel to set its opacity to 50%
    labelWalletHDStatusIcon->setEnabled(hdEnabled);
}

void SINGUI::setEncryptionStatus(int status)
{
    switch(status)
    {
    case WalletModel::Unencrypted:
        labelWalletEncryptionIcon->hide();
        encryptWalletAction->setChecked(false);
        changePassphraseAction->setEnabled(false);
        // Dash
        //-//unlockWalletAction->setVisible(false);
        //-//lockWalletAction->setVisible(false);
        //
        encryptWalletAction->setEnabled(true);
        break;
    case WalletModel::Unlocked:
        labelWalletEncryptionIcon->show();
        labelWalletEncryptionIcon->setPixmap(platformStyle->SingleColorIcon(":/icons/lock_open1").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelWalletEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>unlocked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        // Dash
        //-//unlockWalletAction->setVisible(false);
        //-//lockWalletAction->setVisible(true);
        //
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    case WalletModel::Locked:
        labelWalletEncryptionIcon->show();
        labelWalletEncryptionIcon->setPixmap(platformStyle->SingleColorIcon(":/icons/lock_closed1").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelWalletEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>locked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    }
}

void SINGUI::updateWalletStatus()
{
    if (!walletFrame) {
        return;
    }
    WalletView * const walletView = walletFrame->currentWalletView();
    if (!walletView) {
        return;
    }
    WalletModel * const walletModel = walletView->getWalletModel();
    setEncryptionStatus(walletModel->getEncryptionStatus());
    setHDStatus(walletModel->wallet().hdEnabled());
}
#endif // ENABLE_WALLET

void SINGUI::updateProxyIcon()
{
    std::string ip_port;
    bool proxy_enabled = clientModel->getProxyInfo(ip_port);

    if (proxy_enabled) {
        if (labelProxyIcon->pixmap() == 0) {
            QString ip_port_q = QString::fromStdString(ip_port);
            labelProxyIcon->setPixmap(platformStyle->SingleColorIcon(":/icons/proxy").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
            labelProxyIcon->setToolTip(tr("Proxy is <b>enabled</b>: %1").arg(ip_port_q));
        } else {
            labelProxyIcon->show();
        }
    } else {
        labelProxyIcon->hide();
    }
}

void SINGUI::showNormalIfMinimized(bool fToggleHidden)
{
    if(!clientModel)
        return;

    if (!isHidden() && !isMinimized() && !GUIUtil::isObscured(this) && fToggleHidden) {
        hide();
    } else {
        GUIUtil::bringToFront(this);
    }
}

void SINGUI::toggleHidden()
{
    showNormalIfMinimized(true);
}

void SINGUI::detectShutdown()
{
    if (m_node.shutdownRequested())
    {
        if(rpcConsole)
            rpcConsole->hide();
        qApp->quit();
    }
}

void SINGUI::showProgress(const QString &title, int nProgress)
{
    if (nProgress == 0)
    {
        progressDialog = new QProgressDialog(title, "", 0, 100);
        progressDialog->setWindowModality(Qt::ApplicationModal);
        progressDialog->setMinimumDuration(0);
        progressDialog->setCancelButton(0);
        progressDialog->setAutoClose(false);
        progressDialog->setValue(0);
    }
    else if (nProgress == 100)
    {
        if (progressDialog)
        {
            progressDialog->close();
            progressDialog->deleteLater();
        }
    }
    else if (progressDialog)
        progressDialog->setValue(nProgress);
}

void SINGUI::setTrayIconVisible(bool fHideTrayIcon)
{
    if (trayIcon)
    {
        trayIcon->setVisible(!fHideTrayIcon);
    }
}

void SINGUI::showModalOverlay()
{
    if (modalOverlay && (progressBar->isVisible() || modalOverlay->isLayerVisible()))
        modalOverlay->toggleVisibility();
}

static bool ThreadSafeMessageBox(SINGUI *gui, const std::string& message, const std::string& caption, unsigned int style)
{
    bool modal = (style & CClientUIInterface::MODAL);
    // The SECURE flag has no effect in the Qt GUI.
    // bool secure = (style & CClientUIInterface::SECURE);
    style &= ~CClientUIInterface::SECURE;
    bool ret = false;
    // In case of modal message, use blocking connection to wait for user to click a button
    QMetaObject::invokeMethod(gui, "message",
                               modal ? GUIUtil::blockingGUIThreadConnection() : Qt::QueuedConnection,
                               Q_ARG(QString, QString::fromStdString(caption)),
                               Q_ARG(QString, QString::fromStdString(message)),
                               Q_ARG(unsigned int, style),
                               Q_ARG(bool*, &ret));
    return ret;
}

void SINGUI::subscribeToCoreSignals()
{
    // Connect signals to client
    m_handler_message_box = m_node.handleMessageBox(boost::bind(ThreadSafeMessageBox, this, _1, _2, _3));
    m_handler_question = m_node.handleQuestion(boost::bind(ThreadSafeMessageBox, this, _1, _3, _4));
}

void SINGUI::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    m_handler_message_box->disconnect();
    m_handler_question->disconnect();
}

void SINGUI::toggleNetworkActive()
{
    m_node.setNetworkActive(!m_node.getNetworkActive());
}

UnitDisplayStatusBarControl::UnitDisplayStatusBarControl(const PlatformStyle *platformStyle) :
    optionsModel(0),
    menu(0)
{
    createContextMenu();
    setToolTip(tr("Unit to show amounts in. Click to select another unit."));
    QList<BitcoinUnits::Unit> units = BitcoinUnits::availableUnits();
    int max_width = 0;
    const QFontMetrics fm(font());
    for (const BitcoinUnits::Unit unit : units)
    {
        max_width = qMax(max_width, fm.width(BitcoinUnits::longName(unit)));
    }
    setMinimumSize(max_width, 0);
    setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    //setStyleSheet(QString("QLabel { color : #BC8F3A; font-weight: bold; }").arg(platformStyle->SingleColor().name()));
    setStyleSheet(QString("QLabel { color : #BC8F3A; font-weight: bold; }"));
}

/** So that it responds to button clicks */
void UnitDisplayStatusBarControl::mousePressEvent(QMouseEvent *event)
{
    onDisplayUnitsClicked(event->pos());
}

/** Creates context menu, its actions, and wires up all the relevant signals for mouse events. */
void UnitDisplayStatusBarControl::createContextMenu()
{
    menu = new QMenu(this);
    for (BitcoinUnits::Unit u : BitcoinUnits::availableUnits())
    {
        QAction *menuAction = new QAction(QString(BitcoinUnits::longName(u)), this);
        menuAction->setData(QVariant(u));
        menu->addAction(menuAction);
    }
    connect(menu,SIGNAL(triggered(QAction*)),this,SLOT(onMenuSelection(QAction*)));
}

/** Lets the control know about the Options Model (and its signals) */
void UnitDisplayStatusBarControl::setOptionsModel(OptionsModel *_optionsModel)
{
    if (_optionsModel)
    {
        this->optionsModel = _optionsModel;

        // be aware of a display unit change reported by the OptionsModel object.
        connect(_optionsModel,SIGNAL(displayUnitChanged(int)),this,SLOT(updateDisplayUnit(int)));

        // initialize the display units label with the current value in the model.
        updateDisplayUnit(_optionsModel->getDisplayUnit());
    }
}

/** When Display Units are changed on OptionsModel it will refresh the display text of the control on the status bar */
void UnitDisplayStatusBarControl::updateDisplayUnit(int newUnits)
{
    setText(BitcoinUnits::longName(newUnits));
}

/** Shows context menu with Display Unit options by the mouse coordinates */
void UnitDisplayStatusBarControl::onDisplayUnitsClicked(const QPoint& point)
{
    QPoint globalPos = mapToGlobal(point);
    menu->exec(globalPos);
}

/** Tells underlying optionsModel to update its current display unit. */
void UnitDisplayStatusBarControl::onMenuSelection(QAction* action)
{
    if (action)
    {
        optionsModel->setDisplayUnit(action->data());
    }
}

