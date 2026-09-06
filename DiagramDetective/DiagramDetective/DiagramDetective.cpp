#include "DiagramDetective.h"
#include "DiagramScene.h"

DiagramDetective::DiagramDetective(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::DiagramDetectiveClass())
{
    ui->setupUi(this);

    // the DIAGRAM scene: its double-click handler is what creates objects
    auto* diagram = new DiagramScene(this);
    scene = diagram;
    ui->graphicsView->setScene(scene);
    ui->graphicsView->centerOn(0, 0);   // start looking at the middle of the fixed scene rect

    // the panel's Category dropdown drives the scene's ambient category
    connect(ui->graphicsView, &SketchView::categoryChanged, diagram, &DiagramScene::setAmbientCategory);
    connect(ui->graphicsView, &SketchView::categoryDefined, diagram, [diagram](const QString& name, const QStringList& props) {
        diagram->setAmbientCategory(name);
        if (diagram->ambientCategory() != nullptr)
            diagram->ambientCategory()->setProperties(props);
    });
    diagram->setAmbientCategory(ui->graphicsView->category());
}

DiagramDetective::~DiagramDetective()
{
    delete ui;
    // the scene is parented to this window: Qt deletes it with the window
}
