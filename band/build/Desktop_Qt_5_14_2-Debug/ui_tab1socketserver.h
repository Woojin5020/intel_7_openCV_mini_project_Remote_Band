/********************************************************************************
** Form generated from reading UI file 'tab1socketserver.ui'
**
** Created by: Qt User Interface Compiler version 5.14.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TAB1SOCKETSERVER_H
#define UI_TAB1SOCKETSERVER_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_Tab1Socketserver
{
public:
    QHBoxLayout *horizontalLayout_3;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QLabel *label;
    QPushButton *pStart;
    QPushButton *pStop;
    QTextEdit *pTErecvData;
    QHBoxLayout *horizontalLayout_2;

    void setupUi(QWidget *Tab1Socketserver)
    {
        if (Tab1Socketserver->objectName().isEmpty())
            Tab1Socketserver->setObjectName(QString::fromUtf8("Tab1Socketserver"));
        Tab1Socketserver->resize(398, 290);
        horizontalLayout_3 = new QHBoxLayout(Tab1Socketserver);
        horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        label = new QLabel(Tab1Socketserver);
        label->setObjectName(QString::fromUtf8("label"));

        horizontalLayout->addWidget(label);

        pStart = new QPushButton(Tab1Socketserver);
        pStart->setObjectName(QString::fromUtf8("pStart"));

        horizontalLayout->addWidget(pStart);

        pStop = new QPushButton(Tab1Socketserver);
        pStop->setObjectName(QString::fromUtf8("pStop"));

        horizontalLayout->addWidget(pStop);


        verticalLayout->addLayout(horizontalLayout);

        pTErecvData = new QTextEdit(Tab1Socketserver);
        pTErecvData->setObjectName(QString::fromUtf8("pTErecvData"));

        verticalLayout->addWidget(pTErecvData);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));

        verticalLayout->addLayout(horizontalLayout_2);


        horizontalLayout_3->addLayout(verticalLayout);


        retranslateUi(Tab1Socketserver);

        QMetaObject::connectSlotsByName(Tab1Socketserver);
    } // setupUi

    void retranslateUi(QWidget *Tab1Socketserver)
    {
        Tab1Socketserver->setWindowTitle(QCoreApplication::translate("Tab1Socketserver", "Form", nullptr));
        label->setText(QCoreApplication::translate("Tab1Socketserver", "\354\210\230\354\213\240 \353\215\260\354\235\264\355\204\260", nullptr));
        pStart->setText(QCoreApplication::translate("Tab1Socketserver", "Start", nullptr));
        pStop->setText(QCoreApplication::translate("Tab1Socketserver", "Stop", nullptr));
    } // retranslateUi

};

namespace Ui {
    class Tab1Socketserver: public Ui_Tab1Socketserver {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TAB1SOCKETSERVER_H
