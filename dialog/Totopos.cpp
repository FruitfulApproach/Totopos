#include "Totopos.h"
#include "art/DiagramScene.h"
#include "core/AppSettings.h"
#include "dialog/SettingsDialog.h"
#include "dialog/RenameSceneDialog.h"

#include <QMenuBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QTabWidget>
#include <QTabBar>
#include "widget/TabDragBar.h"
#include "core/io/SceneFile.h"
#include "core/history/SceneHistory.h"
#include "tutor/SetupTutor.h"
#include "widget/PropertiesDock.h"
#include "widget/CommutativeEquationsDock.h"
#include "widget/LibraryDock.h"
#include "widget/ApplicableRulesDock.h"
#include "widget/EnglishDock.h"
#include "tutor/ProofTutor.h"
#include "core/Emoji.h"
#include "core/Branding.h"
#include "core/layout/GraphLayoutThread.h"
#include "tutor/TutorSession.h"
#include "tutor/Tutor.h"
#include <QTimer>
#include <QMenu>
#include <QKeySequence>
#include <QSignalBlocker>
#include "widget/SketchView.h"
#include "widget/CanvasActionBar.h"
#include "art/Arrow.h"
#include "core/props/MapsElements.h"
#include "core/rules/Library.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QDir>
#include <QCloseEvent>
#include <QGridLayout>
#include <QSplitter>

namespace
{
    // the ids the pills come back with. A rule's is its name, prefixed, so
    // one string carries both which kind of pill it was and which rule.
    const QString kMapElements = QStringLiteral("map-elements");
    const QString kRulePrefix = QStringLiteral("rule:");
}

QString Document::title() const
{
    // THE NAME, NOT THE FILENAME. What a diagram is called is "snake-lemma";
    // that it is a conjecture and that it is kept in a .totopos file are two
    // further things about it, said in the file's name because a file has
    // nowhere else to say them. A tab reading "snake-lemma.conjecture.totopos"
    // spends its width on the two that never change. What it IS is shown
    // beside the diagram and in the window title; the tab is for telling one
    // diagram from another.
    return path.isEmpty() ? QStringLiteral("Untitled") : SceneFile::baseNameOf(path);
}

QString Document::displayPath() const
{
    return path.isEmpty() ? QStringLiteral("Untitled") : Library::relativePath(path);
}

bool Document::isModified() const
{
    if (scene == nullptr || scene->history() == nullptr)
        return false;
    // Nowhere to save itself: anything drawn at all is something that would be
    // lost, so an Untitled diagram with a history is modified. An empty one is
    // not - there is nothing in it to keep.
    if (path.isEmpty())
        return scene->history()->canUndo();
    return scene->history()->isModified();
}

QString Document::saveStar() const
{
    return isModified() ? QStringLiteral("*") : QString();
}

Totopos::Totopos(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::TotoposClass())
{
    ui->setupUi(this);
    const QIcon ours = Branding::windowIcon();
    setWindowIcon(ours.isNull() ? Emoji::appIcon() : ours);

    // SEVERAL DIAGRAMS AT ONCE, IN ONE GROUP OF TABS OR TWO.
    //
    // A piece of one diagram is carried into another by holding Ctrl and
    // dragging it; hold the drag over a tab and that tab comes to the front,
    // so the two need never be side by side. They CAN be, though - "Split
    // right" puts a diagram in a group of its own beside this one, which is
    // what a split window is for: one diagram to work in and one to look at
    // while doing it.
    //
    // The group built in the .ui file is the first one. The splitter holds
    // it, and holds the second when there is one.
    m_split = new QSplitter(Qt::Horizontal, ui->centralWidget);
    m_split->setChildrenCollapsible(false);
    if (auto* grid = qobject_cast<QGridLayout*>(ui->centralWidget->layout()))
    {
        grid->removeWidget(ui->tabs);
        grid->addWidget(m_split, 0, 0);
    }
    m_split->addWidget(ui->tabs);
    adoptGroup(ui->tabs);
    // Straight onto the member, NOT through setActiveGroup: that one tells
    // the docks to follow the diagram in front, and the docks are built
    // further down. There is nothing to follow yet either - the first
    // diagram is made after all of this - so the front is simply noted.
    m_activeGroup = ui->tabs;

    // the docks come first: the View menu is built by FINDING them
    buildDocks();
    buildMenus();

    AppSettings::instance().apply();   // the grid, the tutor: from the stored settings

    // AND THE WINDOW AS IT WAS LEFT. After the docks are built, because what
    // is restored is WHERE EACH OF THEM SAT, and a dock that does not exist
    // yet cannot be put back. Nothing saved (a first run) leaves the window
    // wherever the system puts it, which is the right answer for a first run.
    {
        AppSettings& settings = AppSettings::instance();
        const QByteArray geometry = settings.windowGeometry();
        if (!geometry.isEmpty())
            restoreGeometry(geometry);
        const QByteArray arrangement = settings.windowState();
        if (!arrangement.isEmpty())
            restoreState(arrangement);
    }

    // WHAT THE MOUSE IS ON, and what it is: "X : left R-module". Dodger blue,
    // because it is neither news nor a complaint - it is the diagram
    // answering a question about itself - and must not be taken for either
    // the messages beside it or an error.
    m_typing = new QLabel(this);
    m_typing->setStyleSheet("color: #1E90FF; font-weight: bold;");
    m_typing->setToolTip("What the mouse is on, and what it is.");
    statusBar()->addPermanentWidget(m_typing);

    // an ordinary message may cover an error for a moment; when it times out
    // the error comes back, because it is still true
    connect(statusBar(), &QStatusBar::messageChanged, this, [this](const QString& shown) {
        Document* document = current();
        if (shown.isEmpty() && document != nullptr && !document->error.isEmpty())
            showError(document->error);
    });

    // the first diagram
    newDocument();

    // The opening tutor: it explains what to draw and takes none of the
    // clicks. Queued, so the view exists to hang its bubble on.
    QTimer::singleShot(0, this, [this] {
        Document* document = current();
        if (document == nullptr)
            return;
        document->view->setStatement(document->scene->statementText());
        if (!Tutor::isEnabled())
            return;
        auto* setup = new SetupTutor(document->scene);
        if (TutorSession* session = setup->teach(document->scene))
            connect(session, &TutorSession::ended, setup, &QObject::deleteLater);
        else
            setup->deleteLater();
    });
}

// ---------------------------------------------------------------- the docks

void Totopos::buildDocks()
{
    m_properties = new PropertiesDock(this);
    addDockWidget(Qt::RightDockWidgetArea, m_properties);
    m_equations = new CommutativeEquationsDock(this);
    addDockWidget(Qt::RightDockWidgetArea, m_equations);
    m_english = new EnglishDock(this);
    addDockWidget(Qt::RightDockWidgetArea, m_english);
    tabifyDockWidget(m_properties, m_equations);
    tabifyDockWidget(m_equations, m_english);
    m_properties->raise();

    m_library = new LibraryDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_library);
    m_library->setWindowTitle(Emoji::library() + "  Library");
    m_library->toggleViewAction()->setIcon(Emoji::icon(Emoji::library()));

    // Under the library, because it is the library read against this diagram:
    // which of those files actually fit what is drawn.
    m_applicable = new ApplicableRulesDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_applicable);
    m_applicable->toggleViewAction()->setIcon(Emoji::icon(Emoji::library()));
    m_applicable->toggleViewAction()->setStatusTip("Every rule in the library that fits this diagram.");
    connect(m_applicable, &ApplicableRulesDock::message, this, [this](const QString& text) {
        statusBar()->setStyleSheet(QString());
        statusBar()->showMessage(text, 6000);
    });
    // a pin was turned on or off, or what fits has changed under the pins
    connect(m_applicable, &ApplicableRulesDock::pinnedChanged, this, &Totopos::refreshCanvasActions);

    m_properties->setWindowTitle(Emoji::properties() + "  Properties");
    m_equations->setWindowTitle(Emoji::equations() + "  Equations");
    m_english->setWindowTitle(Emoji::english() + "  English");
    m_properties->toggleViewAction()->setIcon(Emoji::icon(Emoji::properties()));
    m_equations->toggleViewAction()->setIcon(Emoji::icon(Emoji::equations()));
    m_english->toggleViewAction()->setIcon(Emoji::icon(Emoji::english()));
    m_english->toggleViewAction()->setStatusTip("What the diagram in front says, in words.");

    // opening one from the library is opening a file, in a tab of its own
    // A library file was renamed on disk. Anything holding it open follows -
    // and only now, after the rename, so nothing changes under the user while
    // the dialog is still up.
    connect(m_library, &LibraryDock::renamed, this, [this](const QString& before, const QString& after) {
        for (Document* document : m_documents)
        {
            // by what the paths SAY: `before` no longer exists, and QFileInfo
            // will not call a file that is gone equal to anything
            if (document->path.isEmpty()
             || QString::compare(QFileInfo(document->path).absoluteFilePath(),
                                 QFileInfo(before).absoluteFilePath(), Qt::CaseInsensitive) != 0)
                continue;
            document->path = after;
            updateTabText(document);
            if (isInFront(document))
                updateTitle();
        }
        statusBar()->showMessage(QString("Renamed to %1.").arg(QFileInfo(after).fileName()), 4000);
    });

    // A library FOLDER was renamed, so every file under it moved with it.
    // Matched by prefix rather than by whole path, and with the separator
    // included: renaming "Grp" must not also claim the files in "Grp-old".
    connect(m_library, &LibraryDock::folderRenamed, this, [this](const QString& before, const QString& after) {
        const QString from = QDir::fromNativeSeparators(QFileInfo(before).absoluteFilePath()) + QLatin1Char('/');
        const QString to = QDir::fromNativeSeparators(QFileInfo(after).absoluteFilePath()) + QLatin1Char('/');
        int moved = 0;
        for (Document* document : m_documents)
        {
            if (document->path.isEmpty())
                continue;
            const QString path = QDir::fromNativeSeparators(QFileInfo(document->path).absoluteFilePath());
            if (!path.startsWith(from, Qt::CaseInsensitive))
                continue;
            document->path = QDir::toNativeSeparators(to + path.mid(from.size()));
            updateTabText(document);
            if (isInFront(document))
                updateTitle();
            ++moved;
        }
        statusBar()->showMessage(moved == 0
            ? QString("Renamed to %1.").arg(QFileInfo(after).fileName())
            : QString("Renamed to %1 - %2 open diagram%3 followed.")
                .arg(QFileInfo(after).fileName()).arg(moved).arg(moved == 1 ? "" : "s"), 5000);
    });

    // A library file was taken off the disk. Anything holding it open keeps the
    // diagram - it is in memory - but has nowhere to save itself back to, so
    // the path is let go of and the tab says Untitled.
    connect(m_library, &LibraryDock::removed, this, [this](const QString& path) {
        for (Document* document : m_documents)
        {
            if (document->path.isEmpty()
             || QString::compare(QFileInfo(document->path).absoluteFilePath(),
                                 QFileInfo(path).absoluteFilePath(), Qt::CaseInsensitive) != 0)
                continue;
            document->path.clear();
            updateTabText(document);
            if (isInFront(document))
                updateTitle();
        }
        statusBar()->showMessage(
            QString("%1 is out of the library.").arg(QFileInfo(path).fileName()), 4000);
    });

    // A NEW DIAGRAM ASKED FOR IN THE LIBRARY. The panel settled the name and
    // the folder; making one and putting it in a tab is ours. It is written
    // to disk at once - an empty file is what puts it IN the library, and a
    // diagram that only appeared there once it was first saved would be a
    // thing the user made and could not see.
    connect(m_library, &LibraryDock::createRequested, this, [this](const QString& path) {
        // THAT NAME MAY ALREADY BE A DIAGRAM. Asked for one that is there
        // already - open in a tab, or sitting on the disk - the answer is
        // that diagram, not a second one: the tab that has it comes to the
        // front, and a file that is there is opened rather than written over.
        if (documentFor(path) != nullptr || QFileInfo::exists(path))
        {
            openFromLibrary(path);
            return;
        }

        Document* document = newDocument();   // a tab of its own, and brought to the front
        document->path = path;
        QString error;
        if (!SceneFile::save(document->scene, path, &error))
        {
            QMessageBox::warning(this, "New diagram", error);
            document->path.clear();   // it is not that file: it is not any file
            return;
        }
        document->scene->history()->markSaved();
        updateTabText(document);
        updateTitle();
        m_library->rescan();   // it is on the disk now, so the tree can show it
        statusBar()->showMessage(QString("Made %1.").arg(QFileInfo(path).fileName()), 5000);
    });

    connect(m_library, &LibraryDock::opened, this, [this](const QString& path) {
        openFromLibrary(path);
    });

    // a click in the library lays that file over the diagram in front as a rule
    connect(m_library, &LibraryDock::chosen, this, [this](const QString& path) {
        if (DiagramScene* scene = currentScene())
            scene->beginRule(path);
    });
    connect(m_library, &LibraryDock::applyAllRequested, this, [this] {
        if (DiagramScene* scene = currentScene())
            scene->applyAllMatches();
    });
    connect(m_library, &LibraryDock::stopRequested, this, [this] {
        if (DiagramScene* scene = currentScene())
            scene->endRule();
    });
}

// ---------------------------------------------------------------- the menus

void Totopos::buildMenus()
{
    // File
    QMenu* file = ui->menuBar->addMenu("&File");
    QAction* newAct = file->addAction("&New");
    newAct->setShortcut(QKeySequence::New);
    newAct->setStatusTip("A new diagram, in a tab of its own.");
    QAction* openAct = file->addAction("&Open...");
    openAct->setShortcut(QKeySequence::Open);
    file->addSeparator();
    QAction* saveAct = file->addAction("&Save");
    saveAct->setShortcut(QKeySequence::Save);
    QAction* saveAsAct = file->addAction("Save &As...");
    saveAsAct->setShortcut(QKeySequence::SaveAs);
    file->addSeparator();
    QAction* closeAct = file->addAction("&Close tab");
    closeAct->setShortcut(QKeySequence::Close);
    newAct->setIcon(Emoji::icon(Emoji::newFile()));
    openAct->setIcon(Emoji::icon(Emoji::openFile()));
    saveAct->setIcon(Emoji::icon(Emoji::saveFile()));
    saveAsAct->setIcon(Emoji::icon(Emoji::saveFile()));
    connect(newAct, &QAction::triggered, this, &Totopos::newDiagram);
    connect(openAct, &QAction::triggered, this, &Totopos::openDiagram);
    connect(saveAct, &QAction::triggered, this, &Totopos::saveDiagram);
    connect(saveAsAct, &QAction::triggered, this, &Totopos::saveDiagramAs);
    connect(closeAct, &QAction::triggered, this, [this] { closeDocument(current()); });

    // Edit: the scene's history, and carrying pieces about
    QMenu* edit = ui->menuBar->addMenu("&Edit");
    m_undo = edit->addAction("&Undo");
    m_undo->setShortcut(QKeySequence::Undo);
    m_redo = edit->addAction("&Redo");
    m_redo->setShortcut(QKeySequence::Redo);
    m_undo->setIcon(Emoji::icon(Emoji::undo()));
    m_redo->setIcon(Emoji::icon(Emoji::redo()));
    edit->addSeparator();

    m_cut = edit->addAction("Cu&t");
    m_cut->setShortcut(QKeySequence::Cut);
    m_copy = edit->addAction("&Copy");
    m_copy->setShortcut(QKeySequence::Copy);
    m_copy->setStatusTip("Take the selection away as a piece of diagram - and, beside it, as a sentence.");
    m_paste = edit->addAction("&Paste");
    m_paste->setShortcut(QKeySequence::Paste);
    m_paste->setStatusTip("Put the piece down where the mouse last was. It can have come from another tab.");
    m_duplicate = edit->addAction("&Duplicate");
    m_duplicate->setShortcut(QKeySequence("Ctrl+D"));
    m_duplicate->setStatusTip("A copy beside the original. Ctrl and drag does the same, wherever you let go.");
    edit->addSeparator();
    m_deleteSelection = edit->addAction(Emoji::remove() + "  &Delete");
    m_deleteSelection->setShortcut(QKeySequence::Delete);
    m_selectAll = edit->addAction("Select &all");
    m_selectAll->setShortcut(QKeySequence::SelectAll);

    connect(m_undo, &QAction::triggered, this, [this] {
        if (DiagramScene* scene = currentScene())
        {
            const QString what = scene->history()->undoText();
            scene->history()->undo();
            statusBar()->showMessage(QString("Undone: %1").arg(what), 4000);
        }
    });
    connect(m_redo, &QAction::triggered, this, [this] {
        if (DiagramScene* scene = currentScene())
        {
            const QString what = scene->history()->redoText();
            scene->history()->redo();
            statusBar()->showMessage(QString("Redone: %1").arg(what), 4000);
        }
    });
    connect(m_cut, &QAction::triggered, this, [this] {
        if (DiagramScene* scene = currentScene()) scene->cutSelection();
    });
    connect(m_copy, &QAction::triggered, this, [this] {
        if (DiagramScene* scene = currentScene()) scene->copySelection();
    });
    connect(m_paste, &QAction::triggered, this, [this] {
        if (DiagramScene* scene = currentScene()) scene->paste();
    });
    connect(m_duplicate, &QAction::triggered, this, [this] {
        if (DiagramScene* scene = currentScene()) scene->duplicateSelection();
    });
    connect(m_deleteSelection, &QAction::triggered, this, [this] {
        if (DiagramScene* scene = currentScene()) scene->deleteNodes(scene->selectedNodes());
    });
    connect(m_selectAll, &QAction::triggered, this, [this] {
        if (DiagramScene* scene = currentScene()) scene->selectEverything();
    });

    // View: every dock there is, and getting back to the diagram. The docks
    // are found rather than listed, so one added later needs nothing here.
    QMenu* view = ui->menuBar->addMenu("&View");
    for (QDockWidget* dock : findChildren<QDockWidget*>())
        view->addAction(dock->toggleViewAction());
    view->addSeparator();

    // SIDE BY SIDE. The diagram in front goes into a group of its own beside
    // the rest, so one can be kept in view - the statement being proved, the
    // definition being used - while another is worked on. Pressed again with
    // the window already split, it carries the diagram back.
    QAction* splitRight = view->addAction("Split &right");
    splitRight->setShortcut(QKeySequence("Ctrl+\\"));
    splitRight->setStatusTip("Put the diagram in front beside the others, so both can be seen at once.");
    connect(splitRight, &QAction::triggered, this, [this] { splitCurrent(); });

    QAction* unsplit = view->addAction("&Join the sides");
    unsplit->setStatusTip("Bring every diagram back into one group of tabs.");
    connect(unsplit, &QAction::triggered, this, [this] {
        // Carried one at a time into the first group, which is the one the
        // window is built around; the empty group then goes of its own accord
        // (see dropIfEmpty).
        QTabWidget* home = m_groups.isEmpty() ? nullptr : m_groups.first();
        if (home == nullptr)
            return;
        for (Document* document : m_documents)
            if (document != nullptr && document->group != home)
                moveToOtherGroup(document);
        setActiveGroup(home);
    });
    view->addSeparator();

    QAction* centre = view->addAction(Emoji::centre() + "  &Centre the diagram");
    centre->setShortcut(QKeySequence("Ctrl+Home"));
    centre->setStatusTip("Bring what is drawn back to the middle of the view.");
    connect(centre, &QAction::triggered, this, [this] {
        if (Document* document = current()) document->view->centreOnContents();
    });

    QAction* fit = view->addAction(Emoji::fit() + "  &Fit the diagram");
    fit->setShortcut(QKeySequence("Ctrl+Shift+Home"));
    fit->setStatusTip("Zoom so the whole diagram is in view.");
    connect(fit, &QAction::triggered, this, [this] {
        if (Document* document = current()) document->view->fitContents();
    });

    // IN AND OUT, by the step the wheel uses, on the diagram in front. With
    // the window split "in front" is the side last worked in, which is what
    // current() answers.
    QAction* zoomIn = view->addAction("Zoom &in");
    zoomIn->setShortcuts({ QKeySequence("Ctrl++"), QKeySequence("Ctrl+=") });
    zoomIn->setStatusTip("Draw everything larger.");
    connect(zoomIn, &QAction::triggered, this, [this] {
        if (Document* document = current()) document->view->zoomBy(1.15);
    });

    QAction* zoomOut = view->addAction("Zoom &out");
    zoomOut->setShortcuts({ QKeySequence("Ctrl+-"), QKeySequence("Ctrl+_") });
    zoomOut->setStatusTip("Draw everything smaller.");
    connect(zoomOut, &QAction::triggered, this, [this] {
        if (Document* document = current()) document->view->zoomBy(1.0 / 1.15);
    });

    QAction* actualSize = view->addAction("&Actual size");
    actualSize->setShortcut(QKeySequence("Ctrl+0"));
    connect(actualSize, &QAction::triggered, this, [this] {
        if (Document* document = current())
        {
            document->view->resetZoom();
            document->view->centreOnContents();
        }
    });

    // HOW THE DIAGRAM IS WRITTEN DOWN - one tick per diagram, not one for the
    // window. Each tab is read in whichever notation it was left in, and the
    // tick follows whichever tab is in front (see bindCurrent).
    view->addSeparator();
    // the arrow is built rather than typed: a double arrow in a narrow string
    // literal depends on the compiler's code page, and this does not
    m_classicalNotation = view->addAction(QStringLiteral("Classical &notation:  givens  ")
                                          + QChar(0x21D2) + QStringLiteral("  conclusion"));
    m_classicalNotation->setCheckable(true);
    m_classicalNotation->setShortcut(QKeySequence("Ctrl+Shift+N"));
    m_classicalNotation->setStatusTip("Set the diagram out the long way: what is given in one box, what follows "
                                      "in another, and the rule's name over the arrow between them. The diagram "
                                      "itself does not change - each notation remembers its own layout.");
    connect(m_classicalNotation, &QAction::triggered, this, [this](bool on) {
        if (DiagramScene* scene = currentScene())
            scene->setNotation(on ? DiagramScene::Notation::Classical : DiagramScene::Notation::Succinct);
        syncNotationAction();
    });

    // One entry per layout algorithm, built from the list rather than written
    // out, so a new GraphLayoutThread subclass appears here by being added to
    // GraphLayouts::all() and nowhere else.
    view->addSeparator();
    QMenu* layout = view->addMenu("&Layout");
    layout->setStatusTip("Tidy the diagram up.");
    for (const GraphLayouts::Kind& kind : GraphLayouts::all())
    {
        QAction* action = layout->addAction(kind.title);
        action->setStatusTip(QString("Lay the diagram out: %1.").arg(kind.title.toLower()));
        const QString id = kind.id;
        connect(action, &QAction::triggered, this, [this, id] {
            if (DiagramScene* scene = currentScene()) scene->layOut(id);
        });
    }

    // Chase
    QMenu* chaseMenu = ui->menuBar->addMenu("&Chase");
    QAction* teachMe = chaseMenu->addAction(Emoji::teach() + "  &Teach me this proof");
    teachMe->setStatusTip("Walk the steps this diagram was built from, one at a time.");
    connect(teachMe, &QAction::triggered, this, [this] {
        DiagramScene* scene = currentScene();
        if (scene == nullptr)
            return;
        auto* tutor = new ProofTutor(scene);
        if (TutorSession* session = tutor->teach(scene))
            connect(session, &TutorSession::ended, tutor, &QObject::deleteLater);
        else
            tutor->deleteLater();
    });

    m_startChase = chaseMenu->addAction("Start diagram &chase");
    m_startChase->setShortcuts({ QKeySequence("Ctrl+Shift+Return"), QKeySequence("Ctrl+Shift+Enter") });
    m_startChase->setIcon(Emoji::appIcon());   // the detective again: this is the chase
    connect(m_startChase, &QAction::triggered, this, [this] {
        if (DiagramScene* scene = currentScene()) scene->toggleChase();
    });

    // Tools > Settings... (Ctrl+,), as in Visual Studio
    QMenu* tools = ui->menuBar->addMenu("&Tools");
    QAction* settings = tools->addAction("&Settings...");
    settings->setShortcut(QKeySequence("Ctrl+,"));
    settings->setIcon(Emoji::icon(Emoji::settings()));
    connect(settings, &QAction::triggered, this, &Totopos::openSettings);

    // Help > Tutor mode. It was a checkbox in the properties panel, sitting
    // among questions about the category, which is not what it is about: it
    // decides whether the guided actions EXPLAIN themselves, and that is a
    // question about being helped, not about the diagram.
    QMenu* help = ui->menuBar->addMenu("&Help");
    m_tutorMode = help->addAction(Emoji::teach() + "  &Tutor mode");
    m_tutorMode->setCheckable(true);
    m_tutorMode->setChecked(Tutor::isEnabled());
    m_tutorMode->setStatusTip("Guided actions explain each step with remarks and an arrow; off, they run quietly.");
    connect(m_tutorMode, &QAction::toggled, this, [](bool on) {
        AppSettings::instance().setValue(AppSettings::TutorEnabled, on);
        AppSettings::instance().apply();
    });
    // and it follows the setting wherever else it is changed (Tools > Settings,
    // the sketch panel), so the tick is never telling a different story
    connect(&AppSettings::instance(), &AppSettings::changed, this, [this] {
        const QSignalBlocker block(m_tutorMode);
        m_tutorMode->setChecked(Tutor::isEnabled());
    });
}

// ---------------------------------------------------------------- the documents

void Totopos::adoptGroup(QTabWidget* group)
{
    if (group == nullptr || m_groups.contains(group))
        return;
    group->setDocumentMode(true);
    group->setMovable(true);
    // NO LITTLE CROSSES. Closing a diagram is the one thing on a tab that
    // cannot be half-done, and a target that small, sitting where you reach
    // to SWITCH tabs, is asked for by accident more often than on purpose.
    // It is on the right-click menu instead, where it can say what it closes.
    group->setTabsClosable(false);
    // A file name is as long as it is; the tab is not. Middle, because the
    // ends of a name are what tell two files apart - "kernel.definition" and
    // "kernel.theorem" differ at the end, and eliding the end hides exactly
    // the part that matters.
    group->setElideMode(Qt::ElideMiddle);
    group->tabBar()->setAcceptDrops(true);
    group->tabBar()->setChangeCurrentOnDrag(true);
    group->tabBar()->setUsesScrollButtons(true);
    // Bold on the bar rather than per tab: every tab is a diagram and they
    // are all equally one, so there is nothing for a difference in weight to
    // say.
    QFont tabFont = group->tabBar()->font();
    tabFont.setBold(true);
    group->tabBar()->setFont(tabFont);
    group->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);

    // A TAB CAN BE CARRIED TO THE OTHER SIDE. The bar reports where the tab
    // came from and where it was let go of; what a tab MEANS - a diagram, in
    // a group - is ours to know, so the move is made here.
    const auto carried = [this, group](TabDragBar* source, int from, int at) {
        auto* fromGroup = source == nullptr ? nullptr
                                            : qobject_cast<QTabWidget*>(source->parentWidget());
        if (fromGroup == nullptr || from < 0 || from >= fromGroup->count())
            return;
        moveDocumentTo(documentOf(fromGroup->widget(from)), group, at);
    };
    if (auto* bar = qobject_cast<TabDragBar*>(group->tabBar()))
        connect(bar, &TabDragBar::tabDroppedIn, this, carried);
    // ...and the same again for a tab let go of on the PAGE rather than on
    // the bar, which is what anybody aims at: it goes on the end.
    if (auto* page = qobject_cast<TabGroup*>(group))
        connect(page, &TabGroup::tabDroppedOnPage, this,
                [carried](TabDragBar* source, int from) { carried(source, from, -1); });

    connect(group->tabBar(), &QWidget::customContextMenuRequested, this,
            [this, group](const QPoint& at) { setActiveGroup(group); showTabMenu(at); });
    connect(group, &QTabWidget::currentChanged, this, [this, group](int index) {
        // Switching tabs in a group says which diagram that group shows, and
        // that this group is the one being used - so it comes to the front.
        // A group whose current tab changed because a tab was moved OUT of it
        // has not been used and says nothing, which is what the count guards.
        if (group->count() == 0)
            return;
        setActiveGroup(group);
        if (group == m_activeGroup)
            currentTabChanged(index);
    });
    m_groups << group;
}

QTabWidget* Totopos::makeGroup()
{
    auto* group = new TabGroup(m_split);
    adoptGroup(group);
    m_split->addWidget(group);
    // an even share: a diagram kept for reference is looked at as much as the
    // one being worked in
    const int each = qMax(1, m_split->width() / qMax(1, m_split->count()));
    QList<int> sizes;
    for (int at = 0; at < m_split->count(); ++at)
        sizes << each;
    m_split->setSizes(sizes);
    return group;
}

QTabWidget* Totopos::activeGroup() const
{
    if (!m_activeGroup.isNull() && m_groups.contains(m_activeGroup))
        return m_activeGroup;
    return m_groups.isEmpty() ? ui->tabs : m_groups.first();
}

void Totopos::setActiveGroup(QTabWidget* group)
{
    if (group == nullptr || !m_groups.contains(group) || m_activeGroup == group)
        return;
    m_activeGroup = group;
    bindCurrent();   // the docks, the menus and the title follow the front
    updateTitle();
}

Document* Totopos::documentOf(QWidget* view) const
{
    for (Document* document : m_documents)
        if (document != nullptr && document->view == view)
            return document;
    return nullptr;
}

void Totopos::splitCurrent()
{
    moveToOtherGroup(current());
}

void Totopos::moveToOtherGroup(Document* document)
{
    if (document == nullptr || document->view == nullptr)
        return;
    QTabWidget* from = document->group;
    QTabWidget* to = nullptr;
    for (QTabWidget* group : m_groups)
        if (group != from)
        {
            to = group;
            break;
        }
    if (to == nullptr)
    {
        // THE ONLY DIAGRAM CANNOT BE PUT BESIDE ITSELF. Splitting here would
        // leave one empty group and one diagram, which is the same window
        // with a gap in it.
        if (from != nullptr && from->count() < 2)
        {
            statusBar()->showMessage("Open another diagram to put one beside it.", 4000);
            return;
        }
        to = makeGroup();
    }

    moveDocumentTo(document, to, -1);
}

void Totopos::moveDocumentTo(Document* document, QTabWidget* to, int at)
{
    if (document == nullptr || document->view == nullptr || to == nullptr
     || !m_groups.contains(to))
        return;

    QTabWidget* from = document->group;
    if (from == to)
    {
        // dragged out and brought back to its own bar: a reorder, which is
        // the one thing the bar could already do for itself
        const int was = from->indexOf(document->view);
        if (at >= 0 && at != was)
            from->tabBar()->moveTab(was, at);
        setActiveGroup(to);
        return;
    }

    if (from != nullptr)
        from->removeTab(from->indexOf(document->view));
    const QString text = document->title() + document->saveStar();
    const int index = at >= 0 && at <= to->count()
        ? to->insertTab(at, document->view, text)
        : to->addTab(document->view, text);
    to->setTabToolTip(index, document->displayPath() + document->saveStar());
    document->group = to;
    to->setCurrentIndex(index);
    dropIfEmpty(from);
    setActiveGroup(to);
    bindCurrent();
}

void Totopos::dropIfEmpty(QTabWidget* group)
{
    if (group == nullptr || group->count() > 0 || m_groups.size() < 2)
        return;
    // The group built in the .ui file is kept whatever happens to it: it is
    // the window's own canvas area and the rest of the program reaches it by
    // name. An empty SECOND group is just a gap, and goes.
    if (group == ui->tabs)
        return;
    m_groups.removeAll(group);
    if (m_activeGroup == group)
        m_activeGroup = m_groups.isEmpty() ? nullptr : m_groups.first();
    group->deleteLater();
}

void Totopos::closeDocument(Document* document)
{
    if (document == nullptr)
        return;
    QTabWidget* group = document->group;
    // the last diagram is not closed but emptied: a window with no canvas in
    // it has nothing to offer
    if (m_documents.size() == 1)
    {
        document->scene->clearDiagram();
        document->path.clear();
        document->error.clear();
        updateTabText(document);
        updateTitle();
        statusBar()->showMessage("New diagram.", 3000);
        return;
    }
    m_documents.removeAll(document);
    if (group != nullptr)
        group->removeTab(group->indexOf(document->view));
    delete document->view;   // the scene is its child, and goes with it
    delete document;
    dropIfEmpty(group);
    bindCurrent();
}

Document* Totopos::current() const
{
    // THE DIAGRAM IN FRONT IS THE ONE IN THE GROUP IN FRONT. With the window
    // split there are two current tabs at all times and only one of them is
    // being worked on; which is settled by what was last clicked in.
    QTabWidget* group = activeGroup();
    return group == nullptr ? nullptr : documentOf(group->currentWidget());
}

Document* Totopos::documentFor(const QString& path) const
{
    if (path.isEmpty())
        return nullptr;
    // canonicalFilePath resolves the separators, the case and any link, so
    // that two spellings of one file are recognised as one file. It is empty
    // for a file that is not there, which is why the plain path is the
    // fallback rather than the other way round.
    const QFileInfo wanted(path);
    const QString canonical = wanted.canonicalFilePath();
    for (Document* document : m_documents)
    {
        if (document == nullptr || document->path.isEmpty())
            continue;
        const QFileInfo open(document->path);
        if (!canonical.isEmpty() && open.canonicalFilePath() == canonical)
            return document;
        if (canonical.isEmpty() && open.absoluteFilePath() == wanted.absoluteFilePath())
            return document;
    }
    return nullptr;
}

DiagramScene* Totopos::currentScene() const
{
    Document* document = current();
    return document != nullptr ? document->scene : nullptr;
}

void Totopos::openFromLibrary(const QString& path)
{
    // ALREADY OPEN MEANS ALREADY OPEN. A second tab onto one file is two
    // diagrams that are both it, each with its own history, each able to save
    // over the other. Bring the one that exists to the front instead.
    if (Document* already = documentFor(path))
    {
        if (already->group != nullptr)
        {
            already->group->setCurrentIndex(already->group->indexOf(already->view));
            setActiveGroup(already->group);
        }
        statusBar()->showMessage(QString("%1 is already open.").arg(SceneFile::baseNameOf(path)), 5000);
        return;
    }

    Document* document = current();
    // an untouched Untitled tab is the place for it; otherwise a new one
    if (document == nullptr || !document->path.isEmpty()
        || (document->scene != nullptr && document->scene->history()->canUndo()))
        document = newDocument();
    if (!openInto(document, path))
        return;
    statusBar()->showMessage(
        QString("Opened %1.  Chase > Teach me this proof walks the steps that made it.")
            .arg(SceneFile::baseNameOf(path)), 8000);
}

Document* Totopos::newDocument()
{
    auto* document = new Document();
    QTabWidget* group = activeGroup();
    document->view = new SketchView(group);
    // a press in this canvas brings its group to the front (see eventFilter)
    document->view->viewport()->installEventFilter(this);
    document->scene = new DiagramScene(document->view);
    document->view->setScene(document->scene);
    document->view->centerOn(0, 0);   // the middle of the fixed scene rect

    document->view->setCategory(AppSettings::instance().defaultCategory());
    document->scene->setAmbientCategory(document->view->category());

    wire(document);

    // the tab list and the widget stack are indexed alike: insert into both
    // The list is the diagrams that are OPEN, in no particular order: where
    // each one is shown is its group's business, and asking a group is how
    // its tab is found (see updateTabText). The two used to be one list
    // indexed alike, which two groups of tabs cannot be.
    const int index = group->addTab(document->view, document->title());
    document->group = group;
    m_documents << document;
    group->setCurrentIndex(index);
    bindCurrent();
    return document;
}

void Totopos::wire(Document* document)
{
    DiagramScene* scene = document->scene;
    SketchView* view = document->view;

    // what the scene has to say (an arrow drawn, a refusal) goes to the status
    // bar - but only while this diagram is the one in front
    connect(scene, &DiagramScene::message, this, [this, document](const QString& text) {
        if (!isInFront(document))
            return;
        statusBar()->setStyleSheet(QString());
        statusBar()->showMessage(text, 6000);
    });

    // what the mouse is on goes to the typing label at the foot of the
    // window, and is rubbed out when it leaves
    connect(scene, &DiagramScene::typingHovered, this, [this, document](const QString& typing) {
        if (m_typing != nullptr && isInFront(document))
            m_typing->setText(typing);
    });

    // and what it cannot mean goes there in red, and stays until it is put right
    connect(scene, &DiagramScene::error, this, [this, document](const QString& text) {
        document->error = text;
        if (!isInFront(document))
            return;
        showError(text);
    });

    // the pills along the foot of the canvas follow the selection, and a
    // press on one is handed to whatever that pill stands for
    connect(scene, &QGraphicsScene::selectionChanged, this, [this, document] {
        if (isInFront(document))
            refreshCanvasActions();
    });
    connect(view, &SketchView::canvasActionTriggered, this, [this](const QString& id) {
        if (id == kMapElements)
        {
            DiagramScene* scene = currentScene();
            const QList<Node*> picked = scene != nullptr ? scene->selectedNodes() : QList<Node*>();
            if (picked.size() != 1)
                return;
            if (auto* arrow = dynamic_cast<Arrow*>(picked.first()))
                if (auto* maps = dynamic_cast<MapsElements*>(arrow->prop(MapsElements::Key())))
                    maps->mapDiagram();
            return;
        }
        if (id.startsWith(kRulePrefix) && m_applicable != nullptr)
            m_applicable->applyPinned(id.mid(kRulePrefix.size()));
    });

    // the scene can change notation without being clicked - opening a file
    // saved in the classical view, or clearing the diagram out from under it
    connect(scene, &DiagramScene::notationChanged, this, [this, document](bool) {
        if (isInFront(document))
            syncNotationAction();
    });

    // a piece being carried in: say where it would land
    connect(view, &SketchView::dropTargetChanged, this, [this](const QString& category) {
        if (category.isEmpty())
            statusBar()->clearMessage();
        else
            statusBar()->showMessage(QString("Let go to put it down in %1.").arg(category));
    });
    // and a piece being carried OFF: a scene is not a widget, so the view carries it
    connect(scene, &DiagramScene::fragmentDragRequested, view, &SketchView::carryFragment);

    // ...AND IS NO LONGER SETTLED BY THE FIRST THING DRAWN IN IT.
    //
    // The dropdown used to lock the moment anything was on the canvas, on the
    // ground that what is drawn in one category means something else in
    // another. True - and the answer is to remake what is drawn, which is
    // what the swap now does (see DiagramScene::setAmbientCategory). Locked,
    // the one way to put right a canvas that is not the category it says it
    // is was to start again and redraw the diagram.
    auto settleCategory = [view] { view->setCategoryLocked(false); };
    if (scene->history() != nullptr)
        connect(scene->history(), &SceneHistory::changed, view, settleCategory);
    // Every way a node can arrive or leave, not only the ones that make a step
    // of the history: a functor's image, a rule drawing with history
    // suspended, and the swap itself all change whether this is settled.
    connect(scene, &DiagramScene::nodesAdded, view, [settleCategory](const QList<Node*>&) { settleCategory(); });
    connect(scene, &DiagramScene::nodesRemoved, view, [settleCategory](const QList<Node*>&) { settleCategory(); });
    // and the dropdown follows the scene rather than the other way round: a
    // switch the scene REFUSES (the category is settled) answers with the
    // category it still is, and the combo goes back to showing that
    connect(scene, &DiagramScene::ambientCategoryChanged, view, [view, settleCategory](Category* ambient) {
        // WHAT IT IS, not what it is called. A canvas can be renamed and a
        // file keeps the two apart, so a Grp called "BigCat" is a thing to
        // meet - and a dropdown reading it off the NAME said BigCat, which
        // made choosing BigCat look like a no-op on a canvas that was making
        // groups (see Category::categoryKind).
        if (ambient != nullptr)
            view->setCategory(ambient->categoryKind());
        settleCategory();
    });
    settleCategory();

    // the panel's Category dropdown drives the scene's ambient category
    connect(view, &SketchView::categoryChanged, scene, &DiagramScene::setAmbientCategory);
    connect(view, &SketchView::categoryDefined, scene, [scene](const QString& name, const QStringList& props) {
        scene->setAmbientCategory(name);
        if (scene->ambientCategory() != nullptr)
            scene->ambientCategory()->setProperties(props);
    });

    // the panel drives the chase and the commuting claim; the scene answers
    // with the sentence the diagram now makes
    connect(view, &SketchView::chaseRequested, scene, &DiagramScene::toggleChase);
    connect(view, &SketchView::commutesChanged, scene, &DiagramScene::setCommutes);
    connect(scene, &DiagramScene::statementChanged, view, &SketchView::setStatement);
    connect(scene, &DiagramScene::chasingChanged, view, &SketchView::setChasing);
    connect(scene, &DiagramScene::commutesChanged, view, &SketchView::setCommutes);
    connect(scene, &DiagramScene::statementKindChanged, view, &SketchView::setStatementKind);
    connect(view, &SketchView::statementKindPicked, scene, [scene](int kind) {
        scene->setStatementKind(DiagramScene::StatementKind(kind));
    });
    connect(view, &SketchView::statementNamed, scene, &DiagramScene::setStatementName);
    connect(scene, &DiagramScene::chasingChanged, this, [this, document](bool chasing) {
        if (isInFront(document))
            m_startChase->setText(chasing ? "End the &chase" : "Start diagram &chase");
    });

    // the rule laid over this diagram, and what its history says can be undone
    connect(scene, &DiagramScene::ruleChanged, this, [this, document](const QString& name, int matches) {
        if (isInFront(document))
            m_library->setRuleState(name, matches);
    });
    connect(scene->history(), &SceneHistory::changed, this, [this, document] {
        // the asterisk lives here: every change to the history is a change to
        // whether this diagram is what is on disk
        updateTabText(document);
        if (isInFront(document))
        {
            updateTitle();
            syncEditActions();
        }
    });
    connect(scene, &QGraphicsScene::selectionChanged, this, [this, document] {
        if (isInFront(document))
            syncEditActions();
    });
    // While a label is being typed in, the keyboard is the label's: Delete
    // takes out a character, Ctrl+C copies the text. A menu shortcut fires
    // before the scene ever sees the key, so the edit actions are switched off
    // for as long as the editor is open - a disabled action does not swallow
    // its shortcut, and the key reaches the editor.
    connect(scene, &QGraphicsScene::focusItemChanged, this, [this, document] {
        if (isInFront(document))
            syncEditActions();
    });
}

void Totopos::bindCurrent()
{
    // NOTHING TO BIND UNTIL THERE IS SOMETHING TO BIND TO. The docks are
    // built after the tabs, and a group coming to the front - which is what
    // brings us here - can happen while the window is still being put
    // together.
    if (m_properties == nullptr)
        return;

    Document* document = current();
    DiagramScene* scene = document != nullptr ? document->scene : nullptr;

    m_properties->setScene(scene);
    m_properties->setView(document != nullptr ? document->view : nullptr);
    m_equations->setScene(scene);
    m_english->setScene(scene);
    m_library->setScene(scene);
    m_applicable->setScene(scene);

    // a rule is laid over one diagram, not over the window
    if (scene != nullptr)
    {
        m_library->setRuleState(scene->ruleName(), scene->matchCount());
        m_startChase->setText(scene->isChasing() ? "End the &chase" : "Start diagram &chase");
        document->view->setStatement(scene->statementText());
        showError(document->error);
    }
    else
    {
        m_library->setRuleState(QString(), 0);
    }

    updateTitle();
    syncEditActions();
    syncNotationAction();
    refreshCanvasActions();
}

void Totopos::refreshCanvasActions()
{
    Document* document = current();
    if (document == nullptr || document->view == nullptr)
        return;
    CanvasActionBar* bar = document->view->actionBar();
    if (bar == nullptr)
        return;

    QList<CanvasActionBar::Action> actions;

    // ---- what the selection is offering
    //
    // One arrow, picked out on its own. Not two, and not an arrow among other
    // things: "map the elements over" is about a particular arrow, and with
    // several selected there is no answer to WHICH.
    if (DiagramScene* scene = document->scene)
    {
        const QList<Node*> picked = scene->selectedNodes();
        if (picked.size() == 1)
            if (auto* arrow = dynamic_cast<Arrow*>(picked.first()))
                if (auto* maps = dynamic_cast<MapsElements*>(arrow->prop(MapsElements::Key())))
                    if (maps->domain() != nullptr && maps->codomain() != nullptr)
                        actions << CanvasActionBar::Action{
                            kMapElements,
                            QString("Map elements by %1").arg(arrow->effectiveId()),
                            QString("Carry the elements drawn in %1 over into %2, along %3.")
                                .arg(maps->domain()->id(), maps->codomain()->id(), arrow->effectiveId()) };
    }

    // ---- and what is pinned AND fits
    if (m_applicable != nullptr)
        for (const ApplicableRule& rule : m_applicable->pinnedRules())
            actions << CanvasActionBar::Action{
                kRulePrefix + rule.name,
                rule.recognises ? QString("Cite %1").arg(rule.name)
                                : QString("Apply %1").arg(rule.name),
                rule.recognises
                    ? QString("Record that %1 holds here.").arg(rule.name)
                    : QString("Draw what %1 says there is, at the first place it fits.").arg(rule.name) };

    bar->setActions(actions);
    document->view->updateGeometry();
}

void Totopos::syncNotationAction()
{
    if (m_classicalNotation == nullptr)
        return;
    DiagramScene* scene = currentScene();
    m_classicalNotation->setEnabled(scene != nullptr);
    QSignalBlocker quiet(m_classicalNotation);   // setting the tick is not a click
    m_classicalNotation->setChecked(scene != nullptr && scene->isClassical());
}

void Totopos::showTabMenu(const QPoint& at)
{
    QTabWidget* group = activeGroup();
    if (group == nullptr)
        return;
    const int index = group->tabBar()->tabAt(at);
    Document* document = index < 0 ? nullptr : documentOf(group->widget(index));
    if (document == nullptr)
        return;

    QMenu menu(this);
    QAction* copy = menu.addAction(QStringLiteral("Copy path"));
    copy->setToolTip("Put the name this file goes by on the clipboard, written from the library "
                     "folder down.");
    copy->setEnabled(!document->path.isEmpty());
    connect(copy, &QAction::triggered, this, [this, document] {
        const QString shown = document->displayPath();
        QGuiApplication::clipboard()->setText(shown);
        statusBar()->showMessage(QString("Copied: %1").arg(shown), 5000);
    });

    // RENAMING IT IS RENAMING THE FILE, and it is asked the same way it is
    // asked in the library panel: the name and what the diagram is put
    // forward as, which together make the filename. A diagram with nowhere to
    // save itself has no name to change, so it is offered Save As instead -
    // the same gesture, at the moment it first needs a name.
    QAction* rename = menu.addAction(document->path.isEmpty()
        ? QStringLiteral("Save as...")
        : QString("Rename %1...").arg(document->title()));
    rename->setToolTip(document->path.isEmpty()
        ? "This diagram has not been saved, so it has no name to change yet."
        : "Rename the file this diagram is kept in. What it is put forward as - a theorem, a "
          "conjecture - is asked alongside, because the filename says both.");
    connect(rename, &QAction::triggered, this, [this, document] {
        QMetaObject::invokeMethod(this, [this, document] { renameDocument(document); },
                                  Qt::QueuedConnection);
    });

    // SIDE BY SIDE. A diagram is often wanted in view while another is
    // worked on - the statement being proved, the definition being used - and
    // that is what a split window is for. The entry says which way it will
    // go: out to a group of its own, or back into the one group there is.
    const bool split = m_groups.size() > 1;
    QAction* aside = menu.addAction(split
        ? QString("Move %1 to the other side").arg(document->title())
        : QString("Split right: %1 beside the rest").arg(document->title()));
    aside->setToolTip(split
        ? "Carry this diagram across to the other group of tabs."
        : "Put this diagram in a group of its own beside the others, so the two can be seen "
          "at once. Closing its last tab puts the window back together.");
    aside->setEnabled(split || m_documents.size() > 1);
    connect(aside, &QAction::triggered, this, [this, document] {
        QMetaObject::invokeMethod(this, [this, document] { moveToOtherGroup(document); },
                                  Qt::QueuedConnection);
    });

    menu.addSeparator();
    QAction* close = menu.addAction(QString("Close %1").arg(document->title()));
    connect(close, &QAction::triggered, this, [this, document] {
        // queued: the menu is still closing, and this takes the tab it
        // belongs to out from under it
        QMetaObject::invokeMethod(this, [this, document] { closeDocument(document); },
                                  Qt::QueuedConnection);
    });

    menu.exec(group->tabBar()->mapToGlobal(at));
}

void Totopos::renameDocument(Document* document)
{
    if (document == nullptr || !m_documents.contains(document))
        return;   // closed while the menu was still up
    if (document->path.isEmpty())
    {
        // nothing on disk to rename: naming it for the first time IS saving it
        if (isInFront(document))
            saveDiagramAs();
        return;
    }

    const QString before = document->path;
    RenameSceneDialog dialog(before, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QString after = dialog.newPath();
    if (QFileInfo(after).absoluteFilePath() == QFileInfo(before).absoluteFilePath())
        return;   // the same name: nothing to do and nothing to say
    if (QFileInfo::exists(after))
    {
        QMessageBox::warning(this, QStringLiteral("Rename"),
            QString("There is already something called %1 there.").arg(dialog.fileName()));
        return;
    }
    if (!QFile::rename(before, after))
    {
        QMessageBox::warning(this, QStringLiteral("Rename"),
            QString("%1 could not be renamed to %2. Something else may have it open.")
                .arg(QFileInfo(before).fileName(), dialog.fileName()));
        return;
    }

    // THE NAME HAS THE LAST WORD ON WHAT THIS IS, exactly as it does when a
    // diagram is saved under a new name: renaming it to .theorem.totopos is
    // how a conjecture becomes a theorem.
    document->path = after;
    if (document->scene != nullptr)
    {
        const int named = SceneFile::kindFromFileName(after);
        if (named != DiagramScene::Unstated)
            document->scene->setStatementKind(DiagramScene::StatementKind(named));
    }
    updateTabText(document);
    if (isInFront(document))
        updateTitle();
    if (m_library != nullptr)
        m_library->rescan();   // it is under a different name on the disk now
    statusBar()->showMessage(QString("Renamed to %1.").arg(dialog.fileName()), 5000);
}

void Totopos::currentTabChanged(int index)
{
    Q_UNUSED(index);
    bindCurrent();
}

void Totopos::closeTab(int index)
{
    // An index is only ever an index INTO A GROUP - this is what the tab
    // widget's own signals hand over, and they hand it over about the group
    // they belong to.
    QTabWidget* group = activeGroup();
    if (group == nullptr || index < 0 || index >= group->count())
        return;
    closeDocument(documentOf(group->widget(index)));
}

// ---------------------------------------------------------------- the window

void Totopos::showError(const QString& text)
{
    if (text.isEmpty())
    {
        statusBar()->setStyleSheet(QString());
        statusBar()->clearMessage();
        return;
    }
    statusBar()->setStyleSheet("color: #dc2626; font-weight: bold;");
    statusBar()->showMessage(text);
}

void Totopos::syncEditActions()
{
    DiagramScene* scene = currentScene();
    if (scene == nullptr)
        return;
    SceneHistory* history = scene->history();
    m_undo->setEnabled(history->canUndo());
    m_redo->setEnabled(history->canRedo());
    m_undo->setText(history->canUndo() ? QString("&Undo %1").arg(history->undoText()) : QString("&Undo"));
    m_redo->setText(history->canRedo() ? QString("&Redo %1").arg(history->redoText()) : QString("&Redo"));

    const bool editing = scene->isEditingLabel();
    const bool anySelected = !editing && !scene->selectedNodes().isEmpty();
    m_cut->setEnabled(anySelected);
    m_copy->setEnabled(anySelected);
    m_duplicate->setEnabled(anySelected);
    m_deleteSelection->setEnabled(anySelected);
    m_selectAll->setEnabled(!editing);
    m_paste->setEnabled(!editing && DiagramScene::clipboardHasFragment());
}

void Totopos::updateTitle()
{
    Document* document = current();
    if (document == nullptr)
    {
        setWindowTitle(QStringLiteral("Totopos"));
        return;
    }
    // "Totopos ~ Grp/classic/inverses exist.axiom.totopos*" - the name the
    // file goes by, not where it happens to sit on this machine, and an
    // asterisk when what is on screen is not what is on disk.
    setWindowTitle(QString("Totopos ~ %1%2").arg(document->displayPath(), document->saveStar()));
}

void Totopos::updateTabText(Document* document)
{
    if (document == nullptr || document->group == nullptr)
        return;
    const int index = document->group->indexOf(document->view);
    if (index < 0)
        return;
    // The NAME on the tab and the PATH under the pointer. A tab is a few
    // centimetres wide and a library path is not, so the tab says which file
    // and the tooltip says which one of several files of that name - the
    // library has a "composition is defined" in more than one folder.
    document->group->setTabText(index, document->title() + document->saveStar());
    document->group->setTabToolTip(index, document->displayPath() + document->saveStar());
}

void Totopos::newDiagram()
{
    newDocument();
    statusBar()->showMessage("New diagram.", 3000);
}

bool Totopos::openInto(Document* document, const QString& path)
{
    QString error;
    if (!SceneFile::load(document->scene, path, &error))
    {
        QMessageBox::warning(this, "Open diagram", error);
        return false;
    }
    document->path = path;
    // just read off the disk, so it IS what is on the disk, whatever history
    // came in with it
    document->scene->history()->markSaved();
    if (document->scene->ambientCategory() != nullptr)
        document->view->setCategory(document->scene->ambientCategory()->categoryKind());
    updateTabText(document);
    updateTitle();
    return true;
}

void Totopos::openDiagram()
{
    // starting where the last diagram was kept, rather than wherever the
    // program happens to have been started from
    const QString path = QFileDialog::getOpenFileName(this, "Open diagram",
        AppSettings::instance().lastFolder(), SceneFile::filter());
    if (path.isEmpty())
        return;
    // an untouched Untitled tab is the place for it; otherwise a new one
    Document* document = current();
    if (document == nullptr || !document->path.isEmpty() || document->scene->history()->canUndo())
        document = newDocument();
    if (!openInto(document, path))
        return;
    AppSettings::instance().setLastFolder(QFileInfo(path).absolutePath());
    const int steps = document->scene->history()->structural().size();
    statusBar()->showMessage(QString("Opened %1 - %2 step%3 of history.")
        .arg(QFileInfo(path).fileName()).arg(steps).arg(steps == 1 ? "" : "s"), 5000);
}

bool Totopos::saveDiagram()
{
    Document* document = current();
    if (document == nullptr)
        return false;
    if (document->path.isEmpty())
        return saveDiagramAs();
    QString error;
    if (!SceneFile::save(document->scene, document->path, &error))
    {
        QMessageBox::warning(this, "Save diagram", error);
        return false;
    }
    // what is on disk is now what is on screen: the asterisk goes
    document->scene->history()->markSaved();
    updateTabText(document);
    updateTitle();
    statusBar()->showMessage(QString("Saved %1.").arg(QFileInfo(document->path).fileName()), 4000);
    return true;
}

bool Totopos::saveDiagramAs()
{
    Document* document = current();
    if (document == nullptr)
        return false;

    // What a file IS is written in its name: kernel.definition.totopos. The
    // dialog is opened at the name this diagram has earned, so saving a
    // theorem does not quietly leave it looking like a drawing.
    const int kind = int(document->scene->statementKind());
    const QString base = document->path.isEmpty()
        ? (document->scene->statementName().isEmpty()
            ? QStringLiteral("untitled")
            : document->scene->statementName())
        : SceneFile::baseNameOf(document->path);
    const QString suggested = QFileInfo(document->path).absolutePath() + "/" + SceneFile::fileNameFor(base, kind);

    // An untitled diagram is offered the folder the last one was kept in, so
    // a session's work lands together instead of wherever the program was
    // started from.
    const QString startIn = document->path.isEmpty()
        ? QDir(AppSettings::instance().lastFolder()).filePath(SceneFile::fileNameFor(base, kind))
        : suggested;
    QString path = QFileDialog::getSaveFileName(this, "Save diagram", startIn, SceneFile::filter());
    if (path.isEmpty())
        return false;
    if (QFileInfo(path).suffix().isEmpty())
        path += "." + SceneFile::extension();
    AppSettings::instance().setLastFolder(QFileInfo(path).absolutePath());

    // the name chosen has the last word on what this is
    if (const int named = SceneFile::kindFromFileName(path);
        named != DiagramScene::Unstated && named != kind)
    {
        document->scene->setStatementKind(DiagramScene::StatementKind(named));
        statusBar()->showMessage(QString("Saved as %1, so that is what this is now.")
            .arg(DiagramScene::kindName(DiagramScene::StatementKind(named)).toLower()), 6000);
    }

    document->path = path;
    updateTabText(document);
    updateTitle();
    return saveDiagram();
}

void Totopos::openSettings()
{
    SettingsDialog dialog(this);
    dialog.exec();
}

bool Totopos::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress)
        for (Document* document : m_documents)
            if (document != nullptr && document->view != nullptr
             && watched == document->view->viewport())
            {
                setActiveGroup(document->group);
                break;
            }
    return QMainWindow::eventFilter(watched, event);
}

void Totopos::closeEvent(QCloseEvent* event)
{
    // Saved before anything else answers the close, so it is written even if
    // something below asks a question and the window ends up staying: the
    // arrangement as it stands is the arrangement to come back to.
    AppSettings::instance().rememberWindow(saveGeometry(), saveState());
    QMainWindow::closeEvent(event);
}

Totopos::~Totopos()
{
    qDeleteAll(m_documents);
    m_documents.clear();
    delete ui;
}
