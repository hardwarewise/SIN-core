#include "statspage.h"
#include "forms/ui_statspage.h"
#include <qt/platformstyle.h>
#include "walletmodel.h"
#include <infinitynodeman.h>

#include <QTimer>




StatsPage::StatsPage(const PlatformStyle *_platformStyle, QWidget *parent) :
    QWidget(parent),
     timer(nullptr),
    ui(new Ui::StatsPage),
    //walletModel(0),
    platformStyle(_platformStyle)
{
    ui->setupUi(this);


 	timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(infinityNodeStat()));
    timer->start(3000);


}


StatsPage::~StatsPage()
{
    if(timer) disconnect(timer, SIGNAL(timeout()), this, SLOT(privateSendStatus()));
    delete ui;
}



void StatsPage::infinityNodeStat()
{
    std::map<COutPoint, CInfinitynode> mapInfinitynodes = infnodeman.GetFullInfinitynodeMap();
    std::map<COutPoint, CInfinitynode> mapInfinitynodesNonMatured = infnodeman.GetFullInfinitynodeNonMaturedMap();
    int total = 0, totalBIG = 0, totalMID = 0, totalLIL = 0, totalUnknown = 0;
    for (auto& infpair : mapInfinitynodes) {
        ++total;
        CInfinitynode inf = infpair.second;
        int sintype = inf.getSINType();
        if (sintype == 10) ++totalBIG;
        else if (sintype == 5) ++totalMID;
        else if (sintype == 1) ++totalLIL;
    }

    int totalNonMatured = 0, totalBIGNonMatured = 0, totalMIDNonMatured = 0, totalLILNonMatured = 0, totalUnknownNonMatured = 0;
    for (auto& infpair : mapInfinitynodesNonMatured) {
        ++totalNonMatured;
        CInfinitynode inf = infpair.second;
        int sintype = inf.getSINType();
        if (sintype == 10) ++totalBIGNonMatured;
        else if (sintype == 5) ++totalMIDNonMatured;
        else if (sintype == 1) ++totalLILNonMatured;
    }

     //QString strTotalNodeText(tr("Total: %1 nodes (Last Scan: %2)").arg(total + totalNonMatured).arg(infnodeman.getLastScanWithLimit()));
    QString strTotalNodeText(tr("%1").arg(total + totalNonMatured));
    QString strLastScanText(tr("%1").arg(infnodeman.getLastScanWithLimit()));
    QString strBIGNodeText(tr("%1").arg(totalBIG));
    QString strMIDNodeText(tr("%1").arg(totalMID));
    QString strLILNodeText(tr("%1").arg(totalLIL));

    QString strBIGNodeQueuedText(tr("Starting %1").arg(totalBIGNonMatured));
    QString strMIDNodeQueuedText(tr("Starting %1").arg(totalMIDNonMatured));
    QString strLILNodeQueuedText(tr("Starting %1").arg(totalLILNonMatured));

    ui->labelStatisticTotalNode->setText(strTotalNodeText);
    ui->labelStatisticLastScan->setText(strLastScanText);
    ui->labelBIGNode->setText(strBIGNodeText);
    ui->labelMIDNode->setText(strMIDNodeText);
    ui->labelLILNode->setText(strLILNodeText);

    ui->labelBIGNodeQueued->setText(strBIGNodeQueuedText);
    ui->labelMIDNodeQueued->setText(strMIDNodeQueuedText);
    ui->labelLILNodeQueued->setText(strLILNodeQueuedText);

    //QString strBIGNodeROIText(tr("ROI %1 days").arg(infnodeman.getRoi(10, totalBIG)));
    //QString strMIDNodeROIText(tr("ROI %1 days").arg(infnodeman.getRoi(5, totalMID)));
    //QString strLILNodeROIText(tr("ROI %1 days").arg(infnodeman.getRoi(1, totalLIL)));

    //ui->labelBIGNodeRoi->setText(strBIGNodeROIText);
    //ui->labelMIDNodeRoi->setText(strMIDNodeROIText);
    //ui->labelLILNodeRoi->setText(strLILNodeROIText);

    QString strBIGNodeSTMText(tr("Payment Round\n%1").arg(infnodeman.getLastStatement(10)));
    QString strMIDNodeSTMText(tr("Payment Round\n%1").arg(infnodeman.getLastStatement(5)));
    QString strLILNodeSTMText(tr("Payment Round\n%1").arg(infnodeman.getLastStatement(1)));

    ui->labelBIGNodeSTM->setText(strBIGNodeSTMText);
    ui->labelMIDNodeSTM->setText(strMIDNodeSTMText);
    ui->labelLILNodeSTM->setText(strLILNodeSTMText);

}


