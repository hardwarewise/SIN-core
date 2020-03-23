include "ui_instaswap.h"
#include "ui_instaswap.h"

Instaswap::Instaswap(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Instaswap)
{
    ui->setupUi(this);
}

Instaswap::~Instaswap()
{
    delete ui;
}
