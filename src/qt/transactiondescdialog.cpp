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

        ui->descFrame->setStyleSheet("background-color: #172F49");
        ui->detailText->setStyleSheet("QTextEdit {color: white; background-color: transparent; border-radius : 12px; margin: 10px;} QToolTip { color: #000000; background-color: #ffffff; border: 0px;}");
        ui->buttonBox->button(QDialogButtonBox::Close)->setStyleSheet("border: 1px solid #74B2FE; background-color: #061834; color: #FFFFFF;");
        
    setWindowTitle(tr("Details for %1").arg(idx.data(TransactionTableModel::TxHashRole).toString()));
    QString desc = idx.data(TransactionTableModel::LongDescriptionRole).toString();
    ui->detailText->setHtml(desc);
   
}

TransactionDescDialog::~TransactionDescDialog()
{
    delete ui;
}
