#include "mainwidget.h"
#include "ui_mainwidget.h"

MainWidget::MainWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MainWidget)
{
    ui->setupUi(this);
    pTab1SocketServer = new Tab1Socketserver(ui->pTab1);
    ui->pTab1->setLayout(pTab1SocketServer->layout());
}

MainWidget::~MainWidget()
{
    delete ui;
}

