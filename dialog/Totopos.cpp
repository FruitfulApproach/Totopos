#include "Totopos.h"
#include "art/DiagramScene.h"
#include "core/AppSettings.h"
#include "dialog/SettingsDialog.h"

#include <QMenuBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QTabWidget>
#include <QTabBar>
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

namespace
{
    // the ids the pills come back with. A rule's is its name, prefixed, so
    // one string carries both which kind of pill it was and which rule.
    const QString kMapElements = QStringLiteral("map-elements");
    const QString kRulePrefix = QStringLiteral("rule:");
}

QString Document::title() const
{
    return path.isEmpty() ? QStringLiteral("Untitled") : QFileInfo(path).fileName();
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

    // Several diagrams at once, each its own tab. A piece of one is carried
    // into another by holding Ctrl and dragging it; hold the drag over a tab
    // and that tab comes to the front, so the two need never be side by side.
    ui->tabs->setDocumentMode(true);
    ui->tabs->setMovable(true);
    // NO LITTLE CROSSES. Closing a diagram is the one thing on a tab that
    // cannot be half-done, and a target that small, sitting where you reach
    // to SWITCH tabs, is asked for by accident more often than on purpose.
    // It is on the right-click menu instead, where it can say what it closes.
    ui->tabs->setTabsClosable(false);
    // A file name is as long as it is; the tab is not. Middle, because the
    // ends of a name are what tell two files apart - "kernel.definition" and
    // "kernel.theorem" differ at the end, and eliding the end hides exactly
    // the part that matters.
    ui->tabs->setElideMode(Qt::ElideMiddle);
    ui->tabs->tabBar()->setAcceptDrops(true);
    ui->tabs->tabBar()->setChangeCurrentOnDrag(true);
    ui->tabs->tabBar()->setUsesScrollButtons(true);
    // Bold on the bar rather than per tab: every tab is a diagram and they are
    // all equally one, so there is nothing for a difference in weight to say.
    QFont tabFont = ui->tabs->tabBar()->font();
    tabFont.setBold(true);
    ui->tabs->tabBar()->setFont(tabFont);
    ui->tabs->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tabs->tabBar(), &QWidget::customContextMenuRequested,
            this, &Totopos::showTabMenu);
    connect(ui->tabs, &QTabWidget::currentChanged, this, &Totopos::currentTabChanged);

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

    connect(m_library, &LibraryDock::opened, this, [this](const QString& path) {
        // ALREADY OPEN MEANS ALREADY OPEN. A second tab onto one file is two
        // diagrams that are both it, each with its own history, each able to
        // save over the other. Bring the one that exists to the front instead.
        if (Document* already = documentFor(path))
        {
            ui->tabs->setCurrentIndex(m_documents.indexOf(already));
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
    connect(closeAct, &QAction::triggered, this, [this] { closeTab(ui->tabs->currentIndex()); });

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

Document* Totopos::current() const
{
    const int index = ui->tabs->currentIndex();
    return index >= 0 && index < m_documents.size() ? m_documents.at(index) : nullptr;
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

Document* Totopos::newDocument()
{
    auto* document = new Document();
    document->view = new SketchView(ui->tabs);
    document->scene = new DiagramScene(document->view);
    document->view->setScene(document->scene);
    document->view->centerOn(0, 0);   // the middle of the fixed scene rect

    document->view->setCategory(AppSettings::instance().defaultCategory());
    document->scene->setAmbientCategory(document->view->category());

    wire(document);

    // the tab list and the widget stack are indexed alike: insert into both
    const int index = ui->tabs->addTab(document->view, document->title());
    m_documents.insert(index, document);
    ui->tabs->setCurrentIndex(index);
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

    // ...and is settled by the first thing drawn in it
    auto settleCategory = [view, scene] {
        Category* ambient = scene->ambientCategory();
        view->setCategoryLocked(ambient != nullptr && ambient->holdsAnything());
    };
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
        if (ambient != nullptr)
            view->setCategory(ambient->id());
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
    const int index = ui->tabs->tabBar()->tabAt(at);
    if (index < 0 || index >= m_documents.size())
        return;
    Document* document = m_documents.at(index);

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

    menu.addSeparator();
    QAction* close = menu.addAction(QString("Close %1").arg(document->title()));
    connect(close, &QAction::triggered, this, [this, index] {
        // queued: the menu is still closing, and this takes the tab it
        // belongs to out from under it
        QMetaObject::invokeMethod(this, [this, index] { closeTab(index); }, Qt::QueuedConnection);
    });

    menu.exec(ui->tabs->tabBar()->mapToGlobal(at));
}

void Totopos::currentTabChanged(int index)
{
    Q_UNUSED(index);
    bindCurrent();
}

void Totopos::closeTab(int index)
{
    if (index < 0 || index >= m_documents.size())
        return;
    Document* document = m_documents.at(index);
    // the last tab is not closed but emptied: a window with no canvas in it
    // has nothing to offer
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
    m_documents.removeAt(index);
    ui->tabs->removeTab(index);
    delete document->view;   // the scene is its child, and goes with it
    delete document;
    bindCurrent();
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
    const int index = m_documents.indexOf(document);
    if (index < 0)
        return;
    // The NAME on the tab and the PATH under the pointer. A tab is a few
    // centimetres wide and a library path is not, so the tab says which file
    // and the tooltip says which one of several files of that name - the
    // library has a "composition is defined" in more than one folder.
    ui->tabs->setTabText(index, document->title() + document->saveStar());
    ui->tabs->setTabToolTip(index, document->displayPath() + document->saveStar());
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
        document->view->setCategory(document->scene->ambientCategory()->id());
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
