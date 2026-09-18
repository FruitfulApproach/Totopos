/********************************************************************************
** Form generated from reading UI file 'DiagramDetective.ui'
**
** Created by: Qt User Interface Compiler version 6.11.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_DIAGRAMDETECTIVE_H
#define UI_DIAGRAMDETECTIVE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QWidget>
#include "widget/SketchView.h"

QT_BEGIN_NAMESPACE

class Ui_DiagramDetectiveClass
{
public:
    QWidget *centralWidget;
    QGridLayout *gridLayout;
    QTabWidget *tabs;
    QMenuBar *menuBar;
    QToolBar *mainToolBar;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *DiagramDetectiveClass)
    {
        if (DiagramDetectiveClass->objectName().isEmpty())
            DiagramDetectiveClass->setObjectName("DiagramDetectiveClass");
        DiagramDetectiveClass->resize(880, 554);
        centralWidget = new QWidget(DiagramDetectiveClass);
        centralWidget->setObjectName("centralWidget");
        gridLayout = new QGridLayout(centralWidget);
        gridLayout->setSpacing(6);
        gridLayout->setContentsMargins(11, 11, 11, 11);
        gridLayout->setObjectName("gridLayout");
        tabs = new QTabWidget(centralWidget);
        tabs->setObjectName("tabs");

        gridLayout->addWidget(tabs, 0, 0, 1, 1);

        DiagramDetectiveClass->setCentralWidget(centralWidget);
        menuBar = new QMenuBar(DiagramDetectiveClass);
        menuBar->setObjectName("menuBar");
        menuBar->setGeometry(QRect(0, 0, 880, 33));
        DiagramDetectiveClass->setMenuBar(menuBar);
        mainToolBar = new QToolBar(DiagramDetectiveClass);
        mainToolBar->setObjectName("mainToolBar");
        DiagramDetectiveClass->addToolBar(Qt::ToolBarArea::TopToolBarArea, mainToolBar);
        statusBar = new QStatusBar(DiagramDetectiveClass);
        statusBar->setObjectName("statusBar");
        DiagramDetectiveClass->setStatusBar(statusBar);

        retranslateUi(DiagramDetectiveClass);

        QMetaObject::connectSlotsByName(DiagramDetectiveClass);
    } // setupUi

    void retranslateUi(QMainWindow *DiagramDetectiveClass)
    {
        DiagramDetectiveClass->setWindowTitle(QCoreApplication::translate("DiagramDetectiveClass", "DiagramDetective", nullptr));
    } // retranslateUi

};

namespace Ui {
    class DiagramDetectiveClass: public Ui_DiagramDetectiveClass {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_DIAGRAMDETECTIVE_H
