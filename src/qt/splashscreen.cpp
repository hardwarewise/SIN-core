// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/sin-config.h>
#endif

#include <qt/splashscreen.h>

#include <qt/networkstyle.h>

#include <clientversion.h>
#include <interfaces/handler.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>
#include <util.h>
#include <ui_interface.h>
#include <version.h>

#include <QApplication>
#include <QCloseEvent>
#include <QDesktopWidget>
#include <QPainter>
#include <QRadialGradient>
#include <QPainter>
#include <QMovie>
#include <QLabel>

#include <univalue/include/univalue.h>
#include <QDebug>
#include <QMessageBox>
#include <QDesktopServices>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QUrl>

SplashScreen::SplashScreen(interfaces::Node& node, Qt::WindowFlags f, const NetworkStyle *networkStyle) :
    QWidget(0, f), curAlignment(0), m_node(node), networkVersionManager(0), versionRequest(0)
{
    setWindowFlags(Qt::FramelessWindowHint);
    

    // set sizes
    int versionTextHeight       = 30;
    int statusHeight            = 30;
    int titleAddTextHeight      = 20;
    int paddingLeft             = 14;
    int paddingTop              = 470;
    int titleVersionVSpace      = 17;
    int titleCopyrightVSpace    = 22;

    float fontFactor            = 1.0;
    float devicePixelRatio      = 1.0;

    //++
    networkVersionManager = new QNetworkAccessManager();
    versionRequest = new QNetworkRequest();
    
      // Get the latest SIN-core release and let the user know if they are using the latest version
        // Network request code for the header widget
        QObject::connect(networkVersionManager, &QNetworkAccessManager::finished,
                         this, [=](QNetworkReply *reply) {
                    if (reply->error()) {
                        qDebug() << reply->errorString();
                        return;
                    }

                    // Get the data from the network request
                    QString answer = reply->readAll();

                    UniValue releases(UniValue::VARR);
                    releases.read(answer.toStdString());

                    if (!releases.isArray()) {
                        return;
                    }

                    if (!releases.size()) {
                        return;
                    }

                    // Latest release lives in the first index of the array return from github v3 api
                    auto latestRelease = releases[0];

                    auto keys = latestRelease.getKeys();
                    for (auto key : keys) {
                       if (key == "tag_name") {
                           auto latestVersion = latestRelease["tag_name"].get_str();

                           QRegExp rx("(\\d+).(\\d+).(\\d+).(\\d+)");
                           rx.indexIn(QString::fromStdString(latestVersion));

                           // List the found values
                           QStringList list = rx.capturedTexts();
                           static const int CLIENT_VERSION_MAJOR_INDEX = 1;
                           static const int CLIENT_VERSION_MINOR_INDEX = 2;
                           static const int CLIENT_VERSION_REVISION_INDEX = 3;
                           static const int CLIENT_VERSION_BUILD_INDEX = 4;
                           bool fNewSoftwareFound = false;
                           bool fStopSearch = false;
                           if (list.size() >= 4) {
                               if (CLIENT_VERSION_MAJOR < list[CLIENT_VERSION_MAJOR_INDEX].toInt()) {
                                   fNewSoftwareFound = true;
                               } else {
                                   if (CLIENT_VERSION_MAJOR > list[CLIENT_VERSION_MAJOR_INDEX].toInt()) {
                                       fStopSearch = true;
                                   }
                               }

                               if (!fStopSearch) {
                                   if (CLIENT_VERSION_MINOR < list[CLIENT_VERSION_MINOR_INDEX].toInt()) {
                                       fNewSoftwareFound = true;
                                   } else {
                                       if (CLIENT_VERSION_MINOR > list[CLIENT_VERSION_MINOR_INDEX].toInt()) {
                                           fStopSearch = true;
                                       }
                                   }
                               }

                               if (!fStopSearch) {
                                   if (CLIENT_VERSION_REVISION < list[CLIENT_VERSION_REVISION_INDEX].toInt()) {
                                       fNewSoftwareFound = true;
                                   }else {
                                       if (CLIENT_VERSION_REVISION > list[CLIENT_VERSION_REVISION_INDEX].toInt()) {
                                           fStopSearch = true;
                                       }
                                 }

                               }

                                if (!fStopSearch) {
                                   if (CLIENT_VERSION_BUILD < list[CLIENT_VERSION_BUILD_INDEX].toInt()) {
                                       fNewSoftwareFound = true;
                                   }
                               }
                           }

                           if (fNewSoftwareFound) {
                               
                             
                               QMessageBox msgBox;
                               msgBox.setWindowTitle("SIN-CORE");
                               msgBox.setTextFormat(Qt::RichText);  
                               msgBox.setText("New wallet version found <br> <br> <a href=\"https://github.com/SINOVATEblockchain/SIN-core/releases/\">Please click here to download the latest version.</a>");
                               msgBox.setWindowModality(Qt::NonModal);
                               msgBox.exec();
                                
                           } 
                       }
                    }
                }
        );

        getLatestVersion();
    
    //--

    // define text to place
    QString titleText       = tr("SIN Core");
    QString versionText     = QString("Version %1").arg(QString::fromStdString(FormatFullVersion()));
    QString copyrightText   = QString::fromUtf8(CopyrightHolders(strprintf("\xc2\xA9 %u-%u ", COPYRIGHT_YEAR-2, COPYRIGHT_YEAR)).c_str());
    QString titleAddText    = networkStyle->getTitleAddText();

    QString font            = QApplication::font().toString();

    // create a bitmap according to device pixelratio
    QSize splashSize(480,540);
    pixmap = QPixmap(480,540);

    QPainter pixPaint(&pixmap);
    pixPaint.setPen(QColor("#ffffff"));

    QRect mainRect(QPoint(0,0), splashSize);
    pixPaint.fillRect(mainRect, QColor("#194FA3"));

    // draw background
    //QRect rectBg(QPoint(0, 0), QSize(splashSize.width(), splashSize.height()));
    //QPixmap bg("");
    //pixPaint.drawPixmap(rectBg, bg);
    QMovie *movie = new QMovie(":/images/splash-loading");
    QLabel *labelMovie = new QLabel(this);
    labelMovie->setContentsMargins(0,0,0,0);
    labelMovie->setStyleSheet("background-color:#194FA3");
    labelMovie->setSizePolicy(QSizePolicy::MinimumExpanding,
                     QSizePolicy::MinimumExpanding);
    labelMovie->setText("");
    labelMovie->setMovie(movie);
    labelMovie->setScaledContents(true);
    movie->start();


    pixPaint.setFont(QFont(font, 28*fontFactor, QFont::Bold));
    QRect rectTitle(QPoint(0,0), QSize(splashSize.width(), (splashSize.height() / 2)));
    pixPaint.drawText(paddingLeft,paddingTop,titleText);

    QPoint versionPoint(rectTitle.bottomLeft());

    // draw additional text if special network
    if(!titleAddText.isEmpty())
    {
        QRect titleAddRect(rectTitle.bottomLeft(), QSize(rectTitle.width(), titleAddTextHeight));
        versionPoint = titleAddRect.bottomLeft();
        pixPaint.setFont(QFont(font, 8*fontFactor, QFont::Bold));

    }

    pixPaint.setFont(QFont(font, 15*fontFactor));
    QRect versionRect(versionPoint, QSize(rectTitle.width(), versionTextHeight));
    pixPaint.drawText(paddingLeft,paddingTop+titleVersionVSpace,versionText);


// draw copyright stuff
    {
        pixPaint.setFont(QFont(font, 8*fontFactor));
        const int x = paddingLeft;
        const int y = paddingTop+titleCopyrightVSpace;
        QRect copyrightRect(x, y, pixmap.width() - x, pixmap.height() - y);
        pixPaint.drawText(copyrightRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, copyrightText +"\n\xc2\xA9 2008-2020 The Bitcoin Core Developers");
    }
    pixPaint.end();

    // Set window title


    // Resize window and move to center of desktop, disallow resizing
    QRect r(QPoint(), QSize(pixmap.size().width()/devicePixelRatio,pixmap.size().height()/devicePixelRatio));
    resize(r.size());
    setFixedSize(r.size());
    move(QApplication::desktop()->screenGeometry().center() - r.center());

    subscribeToCoreSignals();
    installEventFilter(this);
}

SplashScreen::~SplashScreen()
{
    unsubscribeFromCoreSignals();
}

bool SplashScreen::eventFilter(QObject * obj, QEvent * ev) {
    if (ev->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(ev);
        if(keyEvent->text()[0] == 'q') {
            m_node.startShutdown();
        }
    }
    return QObject::eventFilter(obj, ev);
}

void SplashScreen::slotFinish(QWidget *mainWin)
{
    Q_UNUSED(mainWin);

    /* If the window is minimized, hide() will be ignored. */
    /* Make sure we de-minimize the splashscreen window before hiding */
    if (isMinimized())
        showNormal();
    hide();
    deleteLater(); // No more need for this
}

static void InitMessage(SplashScreen *splash, const std::string &message)
{
    QMetaObject::invokeMethod(splash, "showMessage",
        Qt::QueuedConnection,
        Q_ARG(QString, QString::fromStdString(message)),
        Q_ARG(int, Qt::AlignBottom|Qt::AlignHCenter),
        Q_ARG(QColor, QColor("#ffffff")));
}

static void ShowProgress(SplashScreen *splash, const std::string &title, int nProgress, bool resume_possible)
{
    InitMessage(splash, title + std::string("\n") +
            (resume_possible ? _("(press q to shutdown and continue later)")
                                : _("press q to shutdown")) +
            strprintf("\n%d", nProgress) + "%");
}
#ifdef ENABLE_WALLET
void SplashScreen::ConnectWallet(std::unique_ptr<interfaces::Wallet> wallet)
{
    m_connected_wallet_handlers.emplace_back(wallet->handleShowProgress(boost::bind(ShowProgress, this, _1, _2, false)));
    m_connected_wallets.emplace_back(std::move(wallet));
}
#endif

void SplashScreen::subscribeToCoreSignals()
{
    // Connect signals to client
    m_handler_init_message = m_node.handleInitMessage(boost::bind(InitMessage, this, _1));
    m_handler_show_progress = m_node.handleShowProgress(boost::bind(ShowProgress, this, _1, _2, _3));
#ifdef ENABLE_WALLET
    m_handler_load_wallet = m_node.handleLoadWallet([this](std::unique_ptr<interfaces::Wallet> wallet) { ConnectWallet(std::move(wallet)); });
#endif
}

void SplashScreen::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    m_handler_init_message->disconnect();
    m_handler_show_progress->disconnect();
    for (auto& handler : m_connected_wallet_handlers) {
        handler->disconnect();
    }
    m_connected_wallet_handlers.clear();
    m_connected_wallets.clear();
}

void SplashScreen::showMessage(const QString &message, int alignment, const QColor &color)
{
    curMessage = message;
    curAlignment = alignment;
    curColor = color;
    update();
}

void SplashScreen::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.drawPixmap(0, 0, pixmap);
    QRect r = rect().adjusted(5, 5, -5, -5);
    painter.setPen(curColor);
    painter.drawText(r, curAlignment, curMessage);
}

void SplashScreen::closeEvent(QCloseEvent *event)
{
    m_node.startShutdown(); // allows an "emergency" shutdown during startup
    event->ignore();
}
void SplashScreen::getLatestVersion()
{
    versionRequest->setUrl(QUrl("https://api.github.com/repos/SINOVATEblockchain/SIN-core/releases"));
    networkVersionManager->get(*versionRequest);
}