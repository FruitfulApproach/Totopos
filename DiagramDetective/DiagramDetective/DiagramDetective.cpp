#include "DiagramDetective.h"
#include "DiagramScene.h"
#include "AppSettings.h"
#include "SettingsDialog.h"

#include <QMenuBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include "io/SceneFile.h"
#include "history/SceneHistory.h"
#include "SetupTutor.h"
#include "PropertiesDock.h"
#include "CommutativeEquationsDock.h"
#include "LibraryDock.h"
#include "ProofTutor.h"
#include "Emoji.h"
#include "TutorSession.h"
#include "Tutor.h"
#include <QTimer>
#include <QMenu>
#include <QKeySequence>

DiagramDetective::DiagramDetective(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::DiagramDetectiveClass())
{
    ui->setupUi(this);
    setWindowIcon(Emoji::appIcon());

    // File
    QMenu* file = ui->menuBar->addMenu("&File");
    QAction* newAct = file->addAction("&New");
    newAct->setShortcut(QKeySequence::New);
    QAction* openAct = file->addAction("&Open...");
    openAct->setShortcut(QKeySequence::Open);
    file->addSeparator();
    QAction* saveAct = file->addAction("&Save");
    saveAct->setShortcut(QKeySequence::Save);
    QAction* saveAsAct = file->addAction("Save &As...");
    saveAsAct->setShortcut(QKeySequence::SaveAs);
    newAct->setIcon(Emoji::icon(Emoji::newFile()));
    openAct->setIcon(Emoji::icon(Emoji::openFile()));
    saveAct->setIcon(Emoji::icon(Emoji::saveFile()));
    saveAsAct->setIcon(Emoji::icon(Emoji::saveFile()));
    connect(newAct, &QAction::triggered, this, &DiagramDetective::newDiagram);
    connect(openAct, &QAction::triggered, this, &DiagramDetective::openDiagram);
    connect(saveAct, &QAction::triggered, this, &DiagramDetective::saveDiagram);
    connect(saveAsAct, &QAction::triggered, this, &DiagramDetective::saveDiagramAs);

    // Edit: the scene's history, one memento per change
    QMenu* edit = ui->menuBar->addMenu("&Edit");
    m_undo = edit->addAction("&Undo");
    m_undo->setShortcut(QKeySequence::Undo);
    m_redo = edit->addAction("&Redo");
    m_redo->setShortcut(QKeySequence::Redo);
    m_undo->setIcon(Emoji::icon(Emoji::undo()));
    m_redo->setIcon(Emoji::icon(Emoji::redo()));

    // View: the properties of whatever is selected in the diagram. Not to be
    // confused with Tools > Settings, which is what the program does in general.
    m_properties = new PropertiesDock(this);
    addDockWidget(Qt::RightDockWidgetArea, m_properties);
    m_equations = new CommutativeEquationsDock(this);
    addDockWidget(Qt::RightDockWidgetArea, m_equations);
    tabifyDockWidget(m_properties, m_equations);
    m_properties->raise();

    m_library = new LibraryDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_library);
    m_library->setWindowTitle(Emoji::library() + "  Library");
    m_library->toggleViewAction()->setIcon(Emoji::icon(Emoji::library()));

    m_properties->setWindowTitle(Emoji::properties() + "  Properties");
    m_equations->setWindowTitle(Emoji::equations() + "  Equations");
    m_properties->toggleViewAction()->setIcon(Emoji::icon(Emoji::properties()));
    m_equations->toggleViewAction()->setIcon(Emoji::icon(Emoji::equations()));

    // View: every dock there is, and getting back to the diagram. The docks
    // are found rather than listed, so one added later needs nothing here.
    QMenu* view = ui->menuBar->addMenu("&View");
    for (QDockWidget* dock : findChildren<QDockWidget*>())
        view->addAction(dock->toggleViewAction());
    view->addSeparator();

    QAction* centre = view->addAction(Emoji::centre() + "  &Centre the diagram");
    centre->setShortcut(QKeySequence("Ctrl+Home"));
    centre->setStatusTip("Bring what is drawn back to the middle of the view.");
    connect(centre, &QAction::triggered, ui->graphicsView, &SketchView::centreOnContents);

    QAction* fit = view->addAction(Emoji::fit() + "  &Fit the diagram");
    fit->setShortcut(QKeySequence("Ctrl+Shift+Home"));
    fit->setStatusTip("Zoom so the whole diagram is in view.");
    connect(fit, &QAction::triggered, ui->graphicsView, &SketchView::fitContents);

    QAction* actualSize = view->addAction("&Actual size");
    actualSize->setShortcut(QKeySequence("Ctrl+0"));
    connect(actualSize, &QAction::triggered, ui->graphicsView, [this] {
        ui->graphicsView->resetZoom();
        ui->graphicsView->centreOnContents();
    });

    // Chase
    QMenu* chaseMenu = ui->menuBar->addMenu("&Chase");
    QAction* teachMe = chaseMenu->addAction(Emoji::teach() + "  &Teach me this proof");
    teachMe->setStatusTip("Walk the steps this diagram was built from, one at a time.");

    QAction* startChase = chaseMenu->addAction("Start diagram &chase");
    startChase->setShortcuts({ QKeySequence("Ctrl+Shift+Return"), QKeySequence("Ctrl+Shift+Enter") });
    startChase->setIcon(Emoji::appIcon());   // the detective again: this is the chase

    // Tools > Settings... (Ctrl+,), as in Visual Studio
    QMenu* tools = ui->menuBar->addMenu("&Tools");
    QAction* settings = tools->addAction("&Settings...");
    settings->setShortcut(QKeySequence("Ctrl+,"));
    settings->setIcon(Emoji::icon(Emoji::settings()));
    connect(settings, &QAction::triggered, this, &DiagramDetective::openSettings);

    AppSettings::instance().apply();   // the grid, the tutor: from the stored settings

    // the DIAGRAM scene: its double-click handler is what creates objects
    auto* diagram = new DiagramScene(this);
    scene = diagram;
    ui->graphicsView->setScene(scene);
    ui->graphicsView->centerOn(0, 0);   // start looking at the middle of the fixed scene rect

    // what the scene has to say (an arrow drawn, a refusal) goes to the status bar
    connect(diagram, &DiagramScene::message, this, [this](const QString& text) {
        statusBar()->setStyleSheet(QString());
        statusBar()->showMessage(text, 6000);
    });

    // and what it cannot mean goes there in red, and stays until it is fixed
    connect(diagram, &DiagramScene::error, this, [this](const QString& text) {
        m_error = text;
        if (text.isEmpty())
        {
            statusBar()->setStyleSheet(QString());
            statusBar()->clearMessage();
            return;
        }
        statusBar()->setStyleSheet("color: #dc2626; font-weight: bold;");
        statusBar()->showMessage(text);
    });
    // an ordinary message may cover the error for a moment; when it times out
    // the error comes back, because it is still true
    connect(statusBar(), &QStatusBar::messageChanged, this, [this](const QString& shown) {
        if (shown.isEmpty() && !m_error.isEmpty())
        {
            statusBar()->setStyleSheet("color: #dc2626; font-weight: bold;");
            statusBar()->showMessage(m_error);
        }
    });

    // the panel's Category dropdown drives the scene's ambient category
    connect(ui->graphicsView, &SketchView::categoryChanged, diagram, &DiagramScene::setAmbientCategory);
    connect(ui->graphicsView, &SketchView::categoryDefined, diagram, [diagram](const QString& name, const QStringList& props) {
        diagram->setAmbientCategory(name);
        if (diagram->ambientCategory() != nullptr)
            diagram->ambientCategory()->setProperties(props);
    });
    ui->graphicsView->setCategory(AppSettings::instance().defaultCategory());
    diagram->setAmbientCategory(ui->graphicsView->category());

    m_diagram = diagram;
    m_properties->setScene(diagram);
    m_properties->setView(ui->graphicsView);
    m_equations->setScene(diagram);
    m_library->setScene(diagram);

    // opening one from the library is opening a file
    connect(m_library, &LibraryDock::opened, this, [this](const QString& path) {
        QString error;
        if (!SceneFile::load(m_diagram, path, &error))
        {
            QMessageBox::warning(this, "Open", error);
            return;
        }
        m_path = path;
        updateTitle();
        statusBar()->showMessage(
            QString("Opened %1.  Chase > Teach me this proof walks the steps that made it.")
                .arg(QFileInfo(path).completeBaseName()), 8000);
    });

    // Every diagram keeps the steps that made it, so any of them can be gone
    // through again - which is what teaching a proof comes to here.
    connect(teachMe, &QAction::triggered, this, [this] {
        auto* tutor = new ProofTutor(m_diagram);
        if (TutorSession* session = tutor->teach(m_diagram))
            connect(session, &TutorSession::ended, tutor, &QObject::deleteLater);
        else
            tutor->deleteLater();
    });

    // the panel drives the chase and the commuting claim; the scene answers
    // with the sentence the diagram now makes
    // one control for both ways: it starts the chase, and it ends it
    connect(startChase, &QAction::triggered, diagram, &DiagramScene::toggleChase);
    connect(ui->graphicsView, &SketchView::chaseRequested, diagram, &DiagramScene::toggleChase);
    connect(ui->graphicsView, &SketchView::commutesChanged, diagram, &DiagramScene::setCommutes);
    connect(diagram, &DiagramScene::statementChanged, ui->graphicsView, &SketchView::setStatement);
    connect(diagram, &DiagramScene::chasingChanged, ui->graphicsView, &SketchView::setChasing);
    connect(diagram, &DiagramScene::commutesChanged, ui->graphicsView, &SketchView::setCommutes);
    connect(diagram, &DiagramScene::statementKindChanged, ui->graphicsView, &SketchView::setStatementKind);
    connect(ui->graphicsView, &SketchView::statementKindPicked, diagram, [diagram](int kind) {
        diagram->setStatementKind(DiagramScene::StatementKind(kind));
    });
    connect(ui->graphicsView, &SketchView::statementNamed, diagram, &DiagramScene::setStatementName);
    connect(diagram, &DiagramScene::chasingChanged, this, [startChase](bool chasing) {
        startChase->setText(chasing ? "End the &chase" : "Start diagram &chase");
    });

    // The opening tutor: it explains what to draw and takes none of the
    // clicks. Queued, so the view exists to hang its bubble on.
    QTimer::singleShot(0, this, [this] {
        ui->graphicsView->setStatement(m_diagram->statementText());
        if (!Tutor::isEnabled())
            return;
        auto* setup = new SetupTutor(m_diagram);
        if (TutorSession* session = setup->teach(m_diagram))
            connect(session, &TutorSession::ended, setup, &QObject::deleteLater);
        else
            setup->deleteLater();
    });

    connect(m_undo, &QAction::triggered, this, [this] {
        const QString what = m_diagram->history()->undoText();
        m_diagram->history()->undo();
        statusBar()->showMessage(QString("Undone: %1").arg(what), 4000);
    });
    connect(m_redo, &QAction::triggered, this, [this] {
        const QString what = m_diagram->history()->redoText();
        m_diagram->history()->redo();
        statusBar()->showMessage(QString("Redone: %1").arg(what), 4000);
    });
    connect(m_diagram->history(), &SceneHistory::changed, this, &DiagramDetective::syncEditActions);
    syncEditActions();
    updateTitle();
}

void DiagramDetective::syncEditActions()
{
    SceneHistory* history = m_diagram->history();
    m_undo->setEnabled(history->canUndo());
    m_redo->setEnabled(history->canRedo());
    m_undo->setText(history->canUndo() ? QString("&Undo %1").arg(history->undoText()) : QString("&Undo"));
    m_redo->setText(history->canRedo() ? QString("&Redo %1").arg(history->redoText()) : QString("&Redo"));
}

void DiagramDetective::updateTitle()
{
    const QString name = m_path.isEmpty() ? QString("Untitled") : QFileInfo(m_path).fileName();
    setWindowTitle(QString("%1 - Diagram Detective").arg(name));
}

void DiagramDetective::newDiagram()
{
    m_diagram->clearDiagram();
    m_path.clear();
    updateTitle();
    statusBar()->showMessage("New diagram.", 3000);
}

void DiagramDetective::openDiagram()
{
    const QString path = QFileDialog::getOpenFileName(this, "Open diagram", QString(), SceneFile::filter());
    if (path.isEmpty())
        return;
    QString error;
    if (!SceneFile::load(m_diagram, path, &error))
    {
        QMessageBox::warning(this, "Open diagram", error);
        return;
    }
    m_path = path;
    updateTitle();
    ui->graphicsView->setCategory(m_diagram->ambientCategory()->id());
    const int steps = m_diagram->history()->structural().size();
    statusBar()->showMessage(QString("Opened %1 - %2 step%3 of history.")
        .arg(QFileInfo(path).fileName()).arg(steps).arg(steps == 1 ? "" : "s"), 5000);
}

bool DiagramDetective::saveDiagram()
{
    if (m_path.isEmpty())
        return saveDiagramAs();
    QString error;
    if (!SceneFile::save(m_diagram, m_path, &error))
    {
        QMessageBox::warning(this, "Save diagram", error);
        return false;
    }
    statusBar()->showMessage(QString("Saved %1.").arg(QFileInfo(m_path).fileName()), 4000);
    return true;
}

bool DiagramDetective::saveDiagramAs()
{
    QString path = QFileDialog::getSaveFileName(this, "Save diagram", QString(), SceneFile::filter());
    if (path.isEmpty())
        return false;
    if (QFileInfo(path).suffix().isEmpty())
        path += "." + SceneFile::extension();
    m_path = path;
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
    delete ui;
    // the scene is parented to this window: Qt deletes it with the window
}
