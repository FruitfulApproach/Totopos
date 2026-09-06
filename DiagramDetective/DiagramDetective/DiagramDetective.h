#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_DiagramDetective.h"
#include <QMouseEvent>
#include <QGraphicsScene>

QT_BEGIN_NAMESPACE
namespace Ui { class DiagramDetectiveClass; };
QT_END_NAMESPACE

class DiagramDetective : public QMainWindow
{
    Q_OBJECT

public:
    DiagramDetective(QWidget *parent = nullptr);
    ~DiagramDetective();


private:
    Ui::DiagramDetectiveClass *ui;

    QGraphicsScene* scene = nullptr;
};

