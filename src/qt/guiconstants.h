// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_GUICONSTANTS_H
#define BITCOIN_QT_GUICONSTANTS_H

/* Milliseconds between model updates */
static const int MODEL_UPDATE_DELAY = 250;

/* AskPassphraseDialog -- Maximum passphrase length */
static const int MAX_PASSPHRASE_SIZE = 1024;

/* SINGUI -- Size of icons in status bar */
static const int STATUSBAR_ICONSIZE = 16;

static const bool DEFAULT_SPLASHSCREEN = true;

/* Invalid field background style */
#define STYLE_INVALID "background:#FF8080"

/* Transaction list -- unconfirmed transaction */
#define COLOR_UNCONFIRMED QColor(71, 88, 125)
/* Transaction list -- negative amount */
#define COLOR_NEGATIVE QColor(107, 128, 175)
/* Transaction list -- bare address (without label) */
#define COLOR_BAREADDRESS QColor(76, 93, 130)
/* Transaction list -- TX status decoration - open until date */
#define COLOR_TX_STATUS_OPENUNTILDATE QColor(32, 32, 255)
/* Transaction list -- TX status decoration - danger, tx needs attention */
#define COLOR_TX_STATUS_DANGER QColor(255, 75, 84)
/* Transaction list -- TX status decoration - default color */
#define COLOR_BLACK QColor(0, 0, 31)


/* Tooltips longer than this (in characters) are converted into rich text,
   so that they can be word-wrapped.
 */
static const int TOOLTIP_WRAP_THRESHOLD = 80;

/* Maximum allowed URI length */
static const int MAX_URI_LENGTH = 255;

/* QRCodeDialog -- size of exported QR Code image */
#define QR_IMAGE_SIZE 300

/* Number of frames in spinner animation */
#define SPINNER_FRAMES 36

#define QAPP_ORG_NAME "SIN"
#define QAPP_ORG_DOMAIN "sinovate.org"
#define QAPP_APP_NAME_DEFAULT "SIN-Qt"
#define QAPP_APP_NAME_TESTNET "SIN-Qt-testnet"
#define QAPP_APP_NAME_REGTEST "SIN-Qt-regtest"

#endif // BITCOIN_QT_GUICONSTANTS_H
