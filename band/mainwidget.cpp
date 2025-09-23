#include "mainwidget.h"
#include "ui_mainwidget.h"
#include <QVBoxLayout>

MainWidget::MainWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MainWidget)
{
    ui->setupUi(this);

    // Tab1: 서버 위젯
    pTab1SocketServer = new Tab1Socketserver(this);
    if (!ui->pTab1->layout()) ui->pTab1->setLayout(new QVBoxLayout);
    ui->pTab1->layout()->addWidget(pTab1SocketServer);

    // Tab2: 믹서 위젯 ★
    m_mixer = new MixerWidget(this);
    if (!ui->pTab2->layout()) ui->pTab2->setLayout(new QVBoxLayout);
    ui->pTab2->layout()->addWidget(m_mixer);

    // 믹서 → 서버 연결 ★
    if (pTab1SocketServer && pTab1SocketServer->serverWidget()) {
        auto *srv = pTab1SocketServer->serverWidget();
        connect(m_mixer, SIGNAL(mixerVolume(QString,int)),
                srv,     SLOT(onMixerVolume(QString,int)));
        connect(m_mixer, SIGNAL(mixerMute(QString,bool)),
                srv,     SLOT(onMixerMute(QString,bool)));
    }
}

MainWidget::~MainWidget()
{
    delete ui;
}
