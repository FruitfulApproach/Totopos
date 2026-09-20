#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_Totopos.h"
#include <QMouseEvent>
#include <QGraphicsScene>
#include <QList>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui { class TotoposClass; };
QT_END_NAMESPACE

class DiagramScene;
class SketchView;
class PropertiesDock;
class CommutativeEquationsDock;
class LibraryDock;
class ApplicableRulesDock;
class EnglishDock;
class QAction;
class QCloseEvent;

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
	// "Grp/classic/inverses exist.axiom.totopos", or Untitled for a diagram
	// that has never been written anywhere
	QString displayPath() const;
	// Changed since it was last saved? A diagram with nowhere to save itself
	// counts as modified the moment anything is drawn in it - there is
	// something to lose and no file holding it.
	bool isModified() const;
	// the asterisk, or nothing
	QString saveStar() const;
};

class Totopos : public QMainWindow
{
    Q_OBJECT

public:
    Totopos(QWidget *parent = nullptr);
    ~Totopos();

protected:
    // WHERE IT WAS AND HOW IT WAS ARRANGED, kept for the next run. Written on
    // the way out rather than as the window is dragged about: what is worth
    // remembering is where it was LEFT.
    void closeEvent(QCloseEvent* event) override;

private slots:
    void openSettings();
    void newDiagram();
    void openDiagram();
    bool saveDiagram();
    bool saveDiagramAs();
    void closeTab(int index);
    // the right-click menu on a tab: close it, or copy the path it goes by
    void showTabMenu(const QPoint& at);
    // the tab in front changed: every dock and every menu follows it
    void currentTabChanged(int index);

private:
    void buildMenus();
    void buildDocks();

    // a fresh tab, wired up and brought to the front
    Document* newDocument();
    // the diagram in front, or nullptr when there is none
    Document* current() const;
    // The tab this file is already open in, or nullptr. Compared as the file
    // system sees it, not as two strings: the library hands out one spelling
    // of a path and a file dialog another, and they are the same file.
    Document* documentFor(const QString& path) const;
    DiagramScene* currentScene() const;
    // point the docks and the window at this one
    void bindCurrent();
    // everything this document says, said to this window
    void wire(Document* document);
    bool isInFront(const Document* document) const { return document != nullptr && document == current(); }

    void updateTitle();
    void updateTabText(Document* document);
    void syncEditActions();
    // put View > Classical notation in step with the diagram in front
    void syncNotationAction();
    // FILL THE PILLS ALONG THE FOOT OF THE CANVAS.
    //
    // Two sources, and the bar is the only place that knows about both: what
    // the SELECTION is offering (one arrow picked out can map its domain's
    // elements over), and which PINNED rules fit the diagram as it stands.
    // Called whenever either could have changed, and cheap when nothing has.
    void refreshCanvasActions();
    void showError(const QString& text);
    // open a file, in a new tab or in this empty one
    bool openInto(Document* document, const QString& path);

    Ui::TotoposClass *ui;

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
    // View > Classical notation. The notation belongs to the DIAGRAM, so this
    // one tick shows the state of whichever tab is in front and is put back in
    // step every time that changes (see syncNotationAction).
    QAction* m_classicalNotation = nullptr;

    PropertiesDock* m_properties = nullptr;
    CommutativeEquationsDock* m_equations = nullptr;
    LibraryDock* m_library = nullptr;
    ApplicableRulesDock* m_applicable = nullptr;
    EnglishDock* m_english = nullptr;
};
