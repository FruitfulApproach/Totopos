#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_DiagramDetective.h"
#include <QMouseEvent>
#include <QGraphicsScene>
#include <QList>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui { class DiagramDetectiveClass; };
QT_END_NAMESPACE

class DiagramScene;
class SketchView;
class PropertiesDock;
class CommutativeEquationsDock;
class LibraryDock;
class ApplicableRulesDock;
class EnglishDock;
class QAction;

// One diagram open in the window: its own canvas, its own scene, its own file
// and its own history. A piece of one can be carried into another - hold a
// drag over a tab and it comes to the front.
struct Document
{
	SketchView* view = nullptr;
	DiagramScene* scene = nullptr;
	QString path;
	QString error;      // what this diagram cannot mean; shown in red while it is in front

	// what the tab says: the file name, or Untitled
	QString title() const;
};

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
    void closeTab(int index);
    // the tab in front changed: every dock and every menu follows it
    void currentTabChanged(int index);

private:
    void buildMenus();
    void buildDocks();

    // a fresh tab, wired up and brought to the front
    Document* newDocument();
    // the diagram in front, or nullptr when there is none
    Document* current() const;
    DiagramScene* currentScene() const;
    // point the docks and the window at this one
    void bindCurrent();
    // everything this document says, said to this window
    void wire(Document* document);
    bool isInFront(const Document* document) const { return document != nullptr && document == current(); }

    void updateTitle();
    void updateTabText(Document* document);
    void syncEditActions();
    void showError(const QString& text);
    // open a file, in a new tab or in this empty one
    bool openInto(Document* document, const QString& path);

    Ui::DiagramDetectiveClass *ui;

    QList<Document*> m_documents;

    QAction* m_undo = nullptr;
    QAction* m_redo = nullptr;
    QAction* m_cut = nullptr;
    QAction* m_copy = nullptr;
    QAction* m_paste = nullptr;
    QAction* m_duplicate = nullptr;
    QAction* m_selectAll = nullptr;
    QAction* m_deleteSelection = nullptr;
    QAction* m_startChase = nullptr;
    // Help > Tutor mode: the tick follows the setting wherever it is changed
    QAction* m_tutorMode = nullptr;

    PropertiesDock* m_properties = nullptr;
    CommutativeEquationsDock* m_equations = nullptr;
    LibraryDock* m_library = nullptr;
    ApplicableRulesDock* m_applicable = nullptr;
    EnglishDock* m_english = nullptr;
};
