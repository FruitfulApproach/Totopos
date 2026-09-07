#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_DiagramDetective.h"
#include <QMouseEvent>
#include <QGraphicsScene>

QT_BEGIN_NAMESPACE
namespace Ui { class DiagramDetectiveClass; };
QT_END_NAMESPACE

class DiagramScene;
class PropertiesDock;
class CommutativeEquationsDock;
class LibraryDock;
class QAction;

class DiagramDetective : public QMainWindow
{
    Q_OBJECT

public:
    DiagramDetective(QWidget *parent = nullptr);
    ~DiagramDetective();


private slots:
    void openSettings();
    void newDiagram();
    void openDiagram();
    bool saveDiagram();
    bool saveDiagramAs();

private:
    void updateTitle();
    void syncEditActions();

    Ui::DiagramDetectiveClass *ui;

    QGraphicsScene* scene = nullptr;
    DiagramScene* m_diagram = nullptr;
    QString m_path;
    QString m_error;   // shown in red until the diagram is put right
    QAction* m_undo = nullptr;
    QAction* m_redo = nullptr;
    PropertiesDock* m_properties = nullptr;
    CommutativeEquationsDock* m_equations = nullptr;
    LibraryDock* m_library = nullptr;
};

