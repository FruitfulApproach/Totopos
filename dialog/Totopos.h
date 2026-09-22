#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_Totopos.h"
#include <QMouseEvent>
#include <QGraphicsScene>
#include <QList>
#include <QString>
#include <QPointer>
#include <QTabWidget>
#include <QLabel>

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
class QSplitter;
class QTabWidget;

// One diagram open in the window: its own canvas, its own scene, its own file
// and its own history. A piece of one can be carried into another - hold a
// drag over a tab and it comes to the front.
struct Document
{
	SketchView* view = nullptr;
	DiagramScene* scene = nullptr;
	// the tab group it is shown in: with the window split there is more than
	// one, and a tab index alone no longer says which tab
	QTabWidget* group = nullptr;
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

    // WHICH SIDE IS BEING WORKED IN. With the window split, a press anywhere
    // in a group - its tab bar or the canvas itself - is what says the work
    // has moved there, and the docks, the menus and the title follow. Qt has
    // no signal for "this view was clicked", so the presses are watched.
    bool eventFilter(QObject* watched, QEvent* event) override;

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

    // ---- SIDE BY SIDE
    //
    // The tabs live in one or more groups, and the groups sit in a splitter.
    // One group is the ordinary case and is exactly what was there before;
    // a second is what "Split right" makes, so a diagram can be kept in view
    // while another is worked on - the reason anybody splits a window.
    //
    // Everything about a document that used to be found by INDEX (the tab at
    // position i is m_documents[i]) is found through the document itself
    // now: it knows which group it is in, and its position is asked of that
    // group. Two groups and one index are not a pair.
    QTabWidget* makeGroup();
    void adoptGroup(QTabWidget* group);
    QTabWidget* activeGroup() const;
    void setActiveGroup(QTabWidget* group);
    Document* documentOf(QWidget* view) const;
    // put the current diagram in a group of its own beside this one
    void splitCurrent();
    // carry a diagram to the other group (making one if there is none)
    void moveToOtherGroup(Document* document);
    // and the general form: into that group, at that position (-1: the end).
    // Dragging a tab across comes through here, and so does the menu entry.
    void moveDocumentTo(Document* document, QTabWidget* to, int at);
    // a group with nothing left in it is taken away, unless it is the last
    void dropIfEmpty(QTabWidget* group);
    void closeDocument(Document* document);
    // A FILE THE LIBRARY ASKED FOR, opened once. Already open means already
    // open: the tab that exists is brought to the front rather than a second
    // one onto the same file, which would be two diagrams that are both it,
    // each able to save over the other.
    void openFromLibrary(const QString& path);
    // rename the file a tab's diagram is kept in (the tab's own menu)
    void renameDocument(Document* document);
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
    QSplitter* m_split = nullptr;
    QList<QTabWidget*> m_groups;
    QPointer<QTabWidget> m_activeGroup;

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
    // WHAT IS BEING POINTED AT, in the status bar: "X : left R-module".
    //
    // A widget of its own rather than showMessage, and for two reasons. The
    // canvas is a node like any other, so the mouse is over SOMETHING nearly
    // all the time and a message would be overwritten before it could be
    // read; and a message that says what the mouse is on would itself be
    // wiped by the next thing the diagram had to say. Side by side, each
    // says its own thing and neither loses.
    QLabel* m_typing = nullptr;

    LibraryDock* m_library = nullptr;
    ApplicableRulesDock* m_applicable = nullptr;
    EnglishDock* m_english = nullptr;
};
