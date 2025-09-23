#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QtConcurrent>

#include "tab1socketserver.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWidget; }
QT_END_NAMESPACE

class MainWidget : public QWidget
{
    Q_OBJECT

public:
    MainWidget(QWidget *parent = nullptr);
    ~MainWidget();

private:
    Ui::MainWidget *ui;
    Tab1Socketserver *pTab1SocketServer;
};
#endif // MAINWIDGET_H
