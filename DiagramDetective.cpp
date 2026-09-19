#include "DiagramDetective.h"
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
#include "core/layout/GraphLayoutThread.h"
#include "tutor/TutorSession.h"
#include "tutor/Tutor.h"
#include <QTimer>
#include <QMenu>
#include <QKeySequence>
#include <QSignalBlocker>

QString Document::title() const
{
    return path.isEmpty() ? QStringLiteral("Untitled") : QFileInfo(path).fileName();
}

DiagramDetective::DiagramDetective(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::DiagramDetectiveClass())
{
    ui->setupUi(this);
    setWindowIcon(Emoji::appIcon());

    // Several diagrams at once, each its own tab. A piece of one is carried
    // into another by holding Ctrl and dragging it; hold the drag over a tab
    // and that tab comes to the front, so the two need never be side by side.
    ui->tabs->setDocumentMode(true);
    ui->tabs->setMovable(true);
    ui->tabs->setTabsClosable(true);
    ui->tabs->tabBar()->setAcceptDrops(true);
    ui->tabs->tabBar()->setChangeCurrentOnDrag(true);
    connect(ui->tabs, &QTabWidget::tabCloseRequested, this, &DiagramDetective::closeTab);
    connect(ui->tabs, &QTabWidget::currentChanged, this, &DiagramDetective::currentTabChanged);

    // the docks come first: the View menu is built by FINDING them
    buildDocks();
    buildMenus();

    AppSettings::instance().apply();   // the grid, the tutor: from the stored settings

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

void DiagramDetective::buildDocks()
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

void DiagramDetective::buildMenus()
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
    connect(newAct, &QAction::triggered, this, &DiagramDetective::newDiagram);
    connect(openAct, &QAction::triggered, this, &DiagramDetective::openDiagram);
    connect(saveAct, &QAction::triggered, this, &DiagramDetective::saveDiagram);
    connect(saveAsAct, &QAction::triggered, this, &DiagramDetective::saveDiagramAs);
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
    connect(settings, &QAction::triggered, this, &DiagramDetective::openSettings);

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

Document* DiagramDetective::current() const
{
    const int index = ui->tabs->currentIndex();
    return index >= 0 && index < m_documents.size() ? m_documents.at(index) : nullptr;
}

DiagramScene* DiagramDetective::currentScene() const
{
    Document* document = current();
    return document != nullptr ? document->scene : nullptr;
}

Document* DiagramDetective::newDocument()
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

void DiagramDetective::wire(Document* document)
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
        updateTabText(document);
        if (isInFront(document))
            syncEditActions();
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

void DiagramDetective::bindCurrent()
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
}

void DiagramDetective::currentTabChanged(int index)
{
    Q_UNUSED(index);
    bindCurrent();
}

void DiagramDetective::closeTab(int index)
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

void DiagramDetective::showError(const QString& text)
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

void DiagramDetective::syncEditActions()
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

void DiagramDetective::updateTitle()
{
    Document* document = current();
    setWindowTitle(QString("%1 - Diagram Detective")
        .arg(document != nullptr ? document->title() : QStringLiteral("Untitled")));
}

void DiagramDetective::updateTabText(Document* document)
{
    const int index = m_documents.indexOf(document);
    if (index < 0)
        return;
    ui->tabs->setTabText(index, document->title());
    ui->tabs->setTabToolTip(index, document->path.isEmpty() ? document->title() : document->path);
}

void DiagramDetective::newDiagram()
{
    newDocument();
    statusBar()->showMessage("New diagram.", 3000);
}

bool DiagramDetective::openInto(Document* document, const QString& path)
{
    QString error;
    if (!SceneFile::load(document->scene, path, &error))
    {
        QMessageBox::warning(this, "Open diagram", error);
        return false;
    }
    document->path = path;
    if (document->scene->ambientCategory() != nullptr)
        document->view->setCategory(document->scene->ambientCategory()->id());
    updateTabText(document);
    updateTitle();
    return true;
}

void DiagramDetective::openDiagram()
{
    const QString path = QFileDialog::getOpenFileName(this, "Open diagram", QString(), SceneFile::filter());
    if (path.isEmpty())
        return;
    // an untouched Untitled tab is the place for it; otherwise a new one
    Document* document = current();
    if (document == nullptr || !document->path.isEmpty() || document->scene->history()->canUndo())
        document = newDocument();
    if (!openInto(document, path))
        return;
    const int steps = document->scene->history()->structural().size();
    statusBar()->showMessage(QString("Opened %1 - %2 step%3 of history.")
        .arg(QFileInfo(path).fileName()).arg(steps).arg(steps == 1 ? "" : "s"), 5000);
}

bool DiagramDetective::saveDiagram()
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
    statusBar()->showMessage(QString("Saved %1.").arg(QFileInfo(document->path).fileName()), 4000);
    return true;
}

bool DiagramDetective::saveDiagramAs()
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

    QString path = QFileDialog::getSaveFileName(this, "Save diagram",
        document->path.isEmpty() ? SceneFile::fileNameFor(base, kind) : suggested, SceneFile::filter());
    if (path.isEmpty())
        return false;
    if (QFileInfo(path).suffix().isEmpty())
        path += "." + SceneFile::extension();

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

void DiagramDetective::openSettings()
{
    SettingsDialog dialog(this);
    dialog.exec();
}

DiagramDetective::~DiagramDetective()
{
    qDeleteAll(m_documents);
    m_documents.clear();
    delete ui;
}
