#include "faqpage.h"
#include "forms/ui_faqpage.h"
#include <qt/platformstyle.h>





FaqPage::FaqPage(const PlatformStyle *_platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FaqPage),
    platformStyle(_platformStyle)
{
    ui->setupUi(this);


 	

}


FaqPage::~FaqPage()
{
    
    delete ui;
}



