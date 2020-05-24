#ifndef FAQPAGE_H
#define FAQPAGE_H
#include "walletmodel.h"
#include <QWidget>

namespace Ui {
class FaqPage;
}

class WalletModel;




class FaqPage :  public QWidget

{
    Q_OBJECT

public:
    explicit FaqPage(const PlatformStyle *platformStyle, QWidget *parent = 0);
    ~FaqPage();

public Q_SLOTS:
    

private:
    Ui::FaqPage *ui;
    const PlatformStyle *platformStyle;



};

#endif // FAQPAGE_H