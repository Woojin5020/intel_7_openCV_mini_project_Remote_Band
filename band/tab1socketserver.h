#ifndef TAB1SOCKETSERVER_H
#define TAB1SOCKETSERVER_H

#include <QWidget>
#include "serverwidget.h"

namespace Ui {
class Tab1Socketserver;
}

class Tab1Socketserver : public QWidget
{
    Q_OBJECT

public:
    explicit Tab1Socketserver(QWidget *parent = nullptr);
    ~Tab1Socketserver();

signals:
    void socketRecvDataSig(QString strRecvData);

private slots:

    void on_pStart_clicked();
    void updateRecvDataSlot(QString);
    void on_pStop_clicked();

private:
    Ui::Tab1Socketserver *ui;
    ServerWidget *pServerWidget;
};

#endif // TAB1SOCKETSERVER_H
