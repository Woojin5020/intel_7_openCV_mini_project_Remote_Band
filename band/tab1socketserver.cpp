#include "tab1socketserver.h"
#include "ui_tab1socketserver.h"

Tab1Socketserver::Tab1Socketserver(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Tab1Socketserver)
{
    ui->setupUi(this);
    pServerWidget = new ServerWidget(this);
    connect(pServerWidget, &ServerWidget::socketRecvDataSig, this, &Tab1Socketserver::updateRecvDataSlot);
}

Tab1Socketserver::~Tab1Socketserver()
{
    delete ui;
}

void Tab1Socketserver::on_pStart_clicked()
{
    pServerWidget->startServer();
}

void Tab1Socketserver::on_pStop_clicked()
{
    pServerWidget->stopServer();
}

void Tab1Socketserver::updateRecvDataSlot(QString strRecvData)
{
   strRecvData.chop(1);   //끝문자 한개 "\n" 제거
   ui->pTErecvData->append(strRecvData);
}
