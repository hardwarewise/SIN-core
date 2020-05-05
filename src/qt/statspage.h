#ifndef STATSPAGE_H
#define STATSPAGE_H
#include "walletmodel.h"
#include <QWidget>

namespace Ui {
class StatsPage;
}

class WalletModel;




class StatsPage :  public QWidget

{
    Q_OBJECT

public:
    explicit StatsPage(const PlatformStyle *platformStyle, QWidget *parent = 0);
    ~StatsPage();
    
public Q_SLOTS:
    void infinityNodeStat();

private:
    Ui::StatsPage *ui;
    QTimer *timer;
    const PlatformStyle *platformStyle;
    

    
};

#endif // STATSPAGE_H
