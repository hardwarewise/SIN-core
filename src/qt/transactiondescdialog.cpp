// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/transactiondescdialog.h>
#include <qt/forms/ui_transactiondescdialog.h>

#include <qt/transactiontablemodel.h>

#include <QModelIndex>
#include <QSettings>
#include <QPushButton>

TransactionDescDialog::TransactionDescDialog(const QModelIndex &idx, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TransactionDescDialog)
{
    ui->setupUi(this);

        this->setStyleSheet("QDialog {background-color: QLinearGradient(x1: 0, y1: 0, x2: 1, y2: 1, stop: 0 #0C2448, stop: 1 #000D20);} QLabel { color: #6f80ab;}");
        ui->detailText->setStyleSheet("QTextBrowser {color:  #6f80ab; background-color: #1A233A; border-radius : 12px; margin: 10px; padding: 10px; border: 1px solid #6f80ab;} QToolTip { color: #000000; background-color: #FFFFE1; border: 1px solid #000000;}");
        
        
    setWindowTitle(tr("Details for %1").arg(idx.data(TransactionTableModel::TxHashRole).toString()));
    QString desc = idx.data(TransactionTableModel::LongDescriptionRole).toString();
    ui->detailText->setHtml(desc);
   
}

TransactionDescDialog::~TransactionDescDialog()
{
    delete ui;
}
