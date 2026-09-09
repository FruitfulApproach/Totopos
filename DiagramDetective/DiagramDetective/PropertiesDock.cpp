#include "PropertiesDock.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QGroupBox>
#include <QSpinBox>
#include <QPushButton>
#include <QScrollArea>

#include <QListWidget>
#include <QEvent>
#include <functional>

#include "DiagramScene.h"
#include "SketchView.h"
#include "Object.h"
#include "Category.h"
#include "Arrow.h"
#include "Functor.h"
#include <QMouseEvent>
#include "ToggleSwitch.h"
#include "Notation.h"
#include "props/MapsElements.h"
#include "history/SceneHistory.h"
#include "history/Mementos.h"

namespace
{
	// One line of the component list: what it is called, how big it is, and a
	// switch saying whether it commutes. It lights the component up as the
	// mouse passes over it, and a double-click takes the view there.
	class ComponentRow : public QWidget
	{
	public:
		ComponentRow(const QString& title, const QString& detail,
		             bool commutes, bool rowsExact, bool columnsExact, QWidget* parent)
			: QWidget(parent)
		{
			auto* layout = new QHBoxLayout(this);
			layout->setContentsMargins(8, 5, 8, 5);
			layout->setSpacing(8);

			auto* name = new QLabel(title, this);
			QFont bold = name->font();
			bold.setBold(true);
			name->setFont(bold);
			name->setToolTip(title);
			layout->addWidget(name);

			auto* size = new QLabel(detail, this);
			size->setEnabled(false);
			layout->addWidget(size);
			layout->addStretch();

			// A connected piece IS a diagram, which is what these three are
			// claims about. A lone object is not a diagram, which is why they
			// are not to be found on one.
			commutesSwitch = compactSwitch(commutes,
				"Every path with the same two ends in this piece is the same arrow.");
			rowsSwitch = compactSwitch(rowsExact,
				"Every row of this piece is exact: at each object along it, the image of the arrow coming "
				"in is the kernel of the arrow going out.");
			columnsSwitch = compactSwitch(columnsExact,
				"The same, down each column. Rows and columns are claimed separately.");
			layout->addWidget(commutesSwitch);
			layout->addWidget(rowsSwitch);
			layout->addWidget(columnsSwitch);
		}

		ToggleSwitch* compactSwitch(bool on, const QString& tip)
		{
			auto* toggle = new ToggleSwitch(this);
			toggle->setCompact(true);
			toggle->setChecked(on);
			toggle->setToolTip(tip);
			return toggle;
		}

		ToggleSwitch* commutesSwitch = nullptr;
		ToggleSwitch* rowsSwitch = nullptr;
		ToggleSwitch* columnsSwitch = nullptr;
		std::function<void(bool)> onHover;
		std::function<void()> onActivate;

	protected:
		void enterEvent(QEnterEvent* event) override
		{
			QWidget::enterEvent(event);
			if (onHover) onHover(true);
		}
		void leaveEvent(QEvent* event) override
		{
			QWidget::leaveEvent(event);
			if (onHover) onHover(false);
		}
		void mouseDoubleClickEvent(QMouseEvent* event) override
		{
			QWidget::mouseDoubleClickEvent(event);
			if (onActivate) onActivate();
		}
	};
}

PropertiesDock::PropertiesDock(QWidget* parent)
	: QDockWidget("Properties", parent)
{
	setObjectName("propertiesDock");
	setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	build();
	refresh();
}

void PropertiesDock::build()
{
	auto* body = new QWidget(this);
	auto* layout = new QVBoxLayout(body);
	layout->setContentsMargins(12, 10, 12, 12);
	layout->setSpacing(10);

	m_header = new QLabel(body);
	QFont bold = m_header->font();
	bold.setBold(true);
	m_header->setFont(bold);
	layout->addWidget(m_header);

	m_hint = new QLabel("Select an object or an arrow in the diagram.", body);
	m_hint->setWordWrap(true);
	m_hint->setEnabled(false);
	layout->addWidget(m_hint);

	// ---- what any node has
	m_nodeBox = new QGroupBox("Node", body);
	auto* nodeForm = new QFormLayout(m_nodeBox);
	m_id = new QLineEdit(m_nodeBox);
	m_id->setToolTip("The label drawn on it. Anything built out of this label follows it.");
	connect(m_id, &QLineEdit::editingFinished, this, [this] { applyId(m_id->text()); });
	nodeForm->addRow("Label", m_id);
	m_exists = new ToggleSwitch(m_nodeBox);
	m_exists->setToolTip("Draw it dotted and read it as the part that is claimed to EXIST.");
	connect(m_exists, &QAbstractButton::toggled, this, [this](bool on) { applyExistsSuch(on); });
	nodeForm->addRow("Exists such", m_exists);
	layout->addWidget(m_nodeBox);

	// ---- what an object has
	m_objectBox = new QGroupBox("Object", body);
	auto* objectForm = new QFormLayout(m_objectBox);
	m_radius = new QSpinBox(m_objectBox);
	m_radius->setRange(0, 60);
	m_radius->setSuffix(" px");
	m_radius->setKeyboardTracking(false);   // one change when the number is settled, not per digit
	m_radius->setToolTip("How round the corners of the frame are. 0 is a plain rectangle. "
	                     "Applies to every object selected.");
	connect(m_radius, &QSpinBox::valueChanged, this, [this](int value) { applyRounding(value); });
	objectForm->addRow("Corner rounding", m_radius);
	layout->addWidget(m_objectBox);

	// ---- the diagram drawn inside the selected node. Exactness is a claim
	// about a diagram, so it belongs to whatever HOLDS one: not to an object
	// with nothing in it, but to any node with a diagram drawn inside.
	m_insideBox = new QGroupBox("Inside", body);
	auto* insideForm = new QFormLayout(m_insideBox);
	m_rowsExact = new ToggleSwitch(m_insideBox);
	m_rowsExact->setToolTip("Every row of the diagram drawn in here is an exact sequence: at each object "
	                        "along it, the image of the arrow coming in is the kernel of the arrow going out.");
	connect(m_rowsExact, &QAbstractButton::toggled, this, [this](bool on) {
		if (m_updating) return;
		if (Category* home = soleDiagramHome()) home->setRowsExactRecorded(on);
	});
	insideForm->addRow("Rows exact", m_rowsExact);
	m_columnsExact = new ToggleSwitch(m_insideBox);
	m_columnsExact->setToolTip("The same, down each column. Rows and columns are claimed separately.");
	connect(m_columnsExact, &QAbstractButton::toggled, this, [this](bool on) {
		if (m_updating) return;
		if (Category* home = soleDiagramHome()) home->setColumnsExactRecorded(on);
	});
	insideForm->addRow("Columns exact", m_columnsExact);
	layout->addWidget(m_insideBox);

	// ---- the pieces the diagram falls into
	m_componentBox = new QGroupBox("Components", body);
	auto* componentLayout = new QVBoxLayout(m_componentBox);
	componentLayout->setContentsMargins(6, 6, 6, 6);
	auto* componentHint = new QLabel("Each piece of the diagram that hangs together. Hover to pick it out, "
	                                 "double-click to go there.", m_componentBox);
	componentHint->setWordWrap(true);
	componentHint->setEnabled(false);
	componentLayout->addWidget(componentHint);
	// what the three switches on each row are
	auto* captions = new QLabel("commutes   rows   cols", m_componentBox);
	captions->setEnabled(false);
	captions->setAlignment(Qt::AlignRight);
	componentLayout->addWidget(captions);

	m_components = new QListWidget(m_componentBox);
	m_components->setAlternatingRowColors(true);
	m_components->setSelectionMode(QAbstractItemView::NoSelection);
	m_components->setMinimumHeight(120);
	componentLayout->addWidget(m_components);
	layout->addWidget(m_componentBox);

	// ---- what an arrow does to what is drawn in its domain
	m_mappingBox = new QGroupBox("Mapping", body);
	auto* mappingForm = new QFormLayout(m_mappingBox);

	m_mappingHint = new QLabel(m_mappingBox);
	m_mappingHint->setWordWrap(true);
	m_mappingHint->setEnabled(false);
	mappingForm->addRow(m_mappingHint);

	m_showImage = new ToggleSwitch(m_mappingBox);
	m_showImage->setToolTip("Put the image away without giving it up: the nodes are hidden, and whatever is "
	                        "drawn inside them comes back with them. Double-clicking the arrow does the same.");
	connect(m_showImage, &QAbstractButton::toggled, this, [this](bool on) {
		if (m_updating) return;
		if (MapsElements* maps = soleMapping()) maps->setImagesVisible(on);
	});
	mappingForm->addRow("Show the image", m_showImage);

	m_live = new ToggleSwitch(m_mappingBox);
	m_live->setToolTip("Keep the codomain in step with the domain: whatever is drawn or deleted there "
	                   "appears or goes here at once.");
	connect(m_live, &QAbstractButton::toggled, this, [this](bool on) {
		if (m_updating) return;
		if (MapsElements* maps = soleMapping()) maps->setLive(on);
	});
	mappingForm->addRow("Live", m_live);

	m_imagine = new ToggleSwitch(m_mappingBox);
	connect(m_imagine, &QAbstractButton::toggled, this, [this](bool on) {
		if (m_updating) return;
		if (MapsElements* maps = soleMapping()) maps->setImaginesPosition(on);
	});
	mappingForm->addRow("Imagine position changes", m_imagine);

	m_reflect = new ToggleSwitch(m_mappingBox);
	connect(m_reflect, &QAbstractButton::toggled, this, [this](bool on) {
		if (m_updating) return;
		if (MapsElements* maps = soleMapping()) maps->setReflectsPosition(on);
	});
	mappingForm->addRow("Reflect position changes", m_reflect);

	m_contravariant = new ToggleSwitch(m_mappingBox);
	connect(m_contravariant, &QAbstractButton::toggled, this, [this](bool on) {
		if (m_updating) return;
		if (MapsElements* maps = soleMapping()) maps->setContravariant(on);
	});
	mappingForm->addRow("Contravariant", m_contravariant);

	m_imagineBends = new ToggleSwitch(m_mappingBox);
	connect(m_imagineBends, &QAbstractButton::toggled, this, [this](bool on) {
		if (m_updating) return;
		if (MapsElements* maps = soleMapping()) maps->setImaginesBends(on);
	});
	mappingForm->addRow("Imagine bend points", m_imagineBends);

	m_reflectBends = new ToggleSwitch(m_mappingBox);
	connect(m_reflectBends, &QAbstractButton::toggled, this, [this](bool on) {
		if (m_updating) return;
		if (MapsElements* maps = soleMapping()) maps->setReflectsBends(on);
	});
	mappingForm->addRow("Reflect bend points", m_reflectBends);

	m_mapNow = new QPushButton("Map the elements now", m_mappingBox);
	m_mapNow->setToolTip("Draw the image of the domain once, without keeping it live.");
	connect(m_mapNow, &QPushButton::clicked, this, [this] {
		if (MapsElements* maps = soleMapping()) maps->mapDiagram();
	});
	mappingForm->addRow(QString(), m_mapNow);
	layout->addWidget(m_mappingBox);

	layout->addStretch();

	auto* scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setWidget(body);
	setWidget(scroll);
}

void PropertiesDock::setScene(DiagramScene* scene)
{
	if (m_scene != nullptr)
		disconnect(m_scene, nullptr, this, nullptr);
	m_scene = scene;
	if (m_scene != nullptr)
	{
		connect(m_scene, &QGraphicsScene::selectionChanged, this, &PropertiesDock::refresh);
		connect(m_scene, &DiagramScene::nodesAdded, this, &PropertiesDock::refresh);
		connect(m_scene, &DiagramScene::nodesRemoved, this, &PropertiesDock::refresh);
		// the pieces change shape whenever an arrow does
		connect(m_scene, &DiagramScene::statementChanged, this, &PropertiesDock::refresh);
	}
	refresh();
}

QList<Node*> PropertiesDock::selection() const
{
	QList<Node*> nodes;
	if (m_scene == nullptr)
		return nodes;
	for (QGraphicsItem* item : m_scene->selectedItems())
		if (auto* node = dynamic_cast<Node*>(item))
			nodes << node;
	return nodes;
}

Category* PropertiesDock::soleDiagramHome() const
{
	const QList<Node*> nodes = selection();
	if (nodes.size() != 1)
		return nullptr;
	auto* category = dynamic_cast<Category*>(nodes.first());
	// only when there IS a diagram in it: an empty object is not a diagram
	return category != nullptr && category->holdsAnything() ? category : nullptr;
}

MapsElements* PropertiesDock::soleMapping() const
{
	const QList<Node*> nodes = selection();
	if (nodes.size() != 1)
		return nullptr;
	auto* arrow = dynamic_cast<Arrow*>(nodes.first());
	if (arrow == nullptr)
		return nullptr;
	auto* maps = dynamic_cast<MapsElements*>(arrow->prop(MapsElements::Key()));
	// only worth showing when both ends can actually hold anything
	if (maps == nullptr || maps->domain() == nullptr || maps->codomain() == nullptr)
		return nullptr;
	return maps;
}


void PropertiesDock::refresh()
{
	m_updating = true;

	const QList<Node*> nodes = selection();
	QList<Node*> objects;
	for (Node* node : nodes)
		if (dynamic_cast<Arrow*>(node) == nullptr)
			objects << node;

	// Nothing picked out, or everything picked out, is a statement about the
	// WHOLE diagram: show the category it is all drawn in.
	Category* ambient = m_scene != nullptr ? m_scene->ambientCategory() : nullptr;
	const bool wholeThing = nodes.isEmpty()
		|| (ambient != nullptr && nodes.size() >= m_scene->labelledNodes().size());
	if (wholeThing && ambient != nullptr)
	{
		m_header->setText(ambient->contextTitle());
		m_hint->hide();
		m_nodeBox->hide();
		m_objectBox->hide();
		m_mappingBox->hide();
		m_insideBox->hide();

		refreshComponents();
		m_updating = false;
		return;
	}
	if (nodes.isEmpty())
	{
		m_header->setText("Nothing selected");
		m_hint->show();
		m_nodeBox->hide();
		m_objectBox->hide();
		m_mappingBox->hide();
		m_insideBox->hide();
		m_componentBox->hide();
		m_updating = false;
		return;
	}
	m_hint->hide();
	m_componentBox->hide();

	if (nodes.size() == 1)
	{
		Node* node = nodes.first();
		m_header->setText(node->contextTitle());   // "R-module S", "R-linear map n"
		m_id->setEnabled(true);
		m_id->setText(node->id());
	}
	else
	{
		m_header->setText(QString("%1 items selected").arg(nodes.size()));
		m_id->setEnabled(false);
		m_id->setText(QString());
	}

	m_nodeBox->show();
	bool allExist = true;
	for (Node* node : nodes)
		allExist = allExist && node->existsSuch();
	m_exists->setChecked(allExist);

	m_objectBox->setVisible(!objects.isEmpty());
	if (!objects.isEmpty())
	{
		m_radius->setValue(int(objects.first()->cornerRadius() + 0.5));
		m_objectBox->setTitle(objects.size() == 1 ? QStringLiteral("Object")
		                                          : QString("Objects (%1)").arg(objects.size()));
	}

	Category* home = soleDiagramHome();
	m_insideBox->setVisible(home != nullptr);
	if (home != nullptr)
	{
		m_insideBox->setTitle(QString("The diagram in %1").arg(home->id()));
		m_rowsExact->setChecked(home->rowsExact());
		m_columnsExact->setChecked(home->columnsExact());
	}

	MapsElements* maps = soleMapping();
	m_mappingBox->setVisible(maps != nullptr);
	if (maps != nullptr)
	{
		const QString dom = maps->domain()->id();
		const QString cod = maps->codomain()->id();
		const QString to = QString(QChar(0x2192));
		m_mappingBox->setTitle(QString("Mapping  %1 %2 %3").arg(dom, to, cod));
		m_mappingHint->setText(QString("What is drawn in %1 appears in %2.").arg(dom, cod));
		m_showImage->setChecked(maps->imagesVisible());
		m_live->setChecked(maps->isLive());
		m_imagine->setChecked(maps->imaginesPosition());
		m_reflect->setChecked(maps->reflectsPosition());
		m_contravariant->setChecked(maps->isContravariant());
		m_contravariant->setToolTip(QString(
			"The image arrows run the other way: the image of f : X %1 Y goes from the image of Y to the "
			"image of X. Hom(.,D) is like this; Hom(A,.) is not.\n\n"
			"Every name is a formula with a hole "
			"in it, and the dot is the hole: a name written without one has it at the end. This one reads "
			"%2, so applying it to A writes %3.")
			.arg(to,
			     MapsElements::formula(maps->arrow() != nullptr ? maps->arrow()->id() : QStringLiteral("H(.,D)")),
			     MapsElements::applied(maps->arrow() != nullptr ? maps->arrow()->id() : QStringLiteral("H(.,D)"), "A")));
		m_imagineBends->setChecked(maps->imaginesBends());
		m_reflectBends->setChecked(maps->reflectsBends());
		m_imagineBends->setToolTip(QString("An arrow bent in %1 is bent the same way in %2: the same number "
		                                   "of control points, in the same places (%1 %3 %2).").arg(dom, cod, to));
		m_reflectBends->setToolTip(QString("Bending an image arrow in %2 bends what it is the image of, back "
		                                   "in %1 (%2 %3 %1).").arg(dom, cod, to));
		m_imagine->setToolTip(QString("Moving an object in %1 moves its image in %2 BY THE SAME AMOUNT "
		                              "(%1 %3 %2). Each still sits where you put it - the image travels with "
		                              "the object rather than being pinned to it.").arg(dom, cod, to));
		m_reflect->setToolTip(QString("Dragging an image in %2 moves what it is the image of, back in %1, by "
		                              "the same amount (%2 %3 %1). With both on, whichever one you drag leads "
		                              "and the other follows - neither answers the other back.").arg(dom, cod, to));
	}

	m_updating = false;
}

void PropertiesDock::applyId(const QString& id)
{
	if (m_updating)
		return;
	const QList<Node*> nodes = selection();
	const QString written = Notation::withScripts(id);
	if (nodes.size() != 1 || nodes.first()->id() == written)
		return;
	Node* node = nodes.first();
	const QString before = node->id();
	node->setId(written);
	node->bindLabelReferences();
	if (m_scene != nullptr)
		m_scene->history()->record(new Renamed(QString("Renamed %1 to %2").arg(before, written), node, before, written));
}

void PropertiesDock::applyExistsSuch(bool on)
{
	if (m_updating)
		return;
	for (Node* node : selection())
		node->setExistsSuchRecorded(on);
}

void PropertiesDock::applyRounding(int radius)
{
	if (m_updating || m_scene == nullptr)
		return;
	for (Node* node : selection())
	{
		if (dynamic_cast<Arrow*>(node) != nullptr)
			continue;
		const qreal before = node->cornerRadius();
		if (qFuzzyCompare(before, qreal(radius)))
			continue;
		node->setCornerRadius(radius);
		m_scene->history()->record(new StyleChanged(
			QString("Rounded %1").arg(node->id().isEmpty() ? QStringLiteral("a node") : node->id()),
			node, node->fill(), node->border(), node->fill(), node->border(), before, radius));
	}
}

void PropertiesDock::refreshComponents()
{
	m_components->clear();
	m_componentBox->show();
	if (m_scene == nullptr)
		return;

	const QList<DiagramScene::Component> pieces = m_scene->components();
	m_componentBox->setTitle(pieces.size() == 1
		? QStringLiteral("Components  (1)")
		: QString("Components  (%1)").arg(pieces.size()));

	for (const DiagramScene::Component& piece : pieces)
	{
		const QString detail = QString("%1 object%2, %3 arrow%4")
			.arg(piece.objects.size()).arg(piece.objects.size() == 1 ? "" : "s")
			.arg(piece.arrows.size()).arg(piece.arrows.size() == 1 ? "" : "s");

		auto* item = new QListWidgetItem(m_components);
		auto* row = new ComponentRow(piece.title, detail, piece.commutes,
		                             piece.rowsExact, piece.columnsExact, m_components);

		// hovering it picks it out of the diagram; double-clicking goes there
		const QList<Node*> objects = piece.objects;
		const QList<Arrow*> arrows = piece.arrows;
		const QRectF bounds = piece.bounds;
		row->onHover = [this, objects, arrows](bool on) { lightUp(objects, arrows, on); };
		row->onActivate = [this, bounds] {
			if (m_view != nullptr)
				m_view->fitTo(bounds);
		};
		// A piece has no memory of its own - it is worked out from the arrows
		// every time - so each claim is written onto its members.
		auto claim = [this, objects, arrows](void (Node::*set)(bool), bool on, bool recheck) {
			if (m_updating)
				return;
			for (Node* node : objects)
				(node->*set)(on);
			for (Arrow* arrow : arrows)
				(arrow->*set)(on);
			if (m_scene != nullptr)
			{
				if (recheck)
					m_scene->checkDiagram();   // a ring here may just have started, or stopped, mattering
				emit m_scene->statementChanged(m_scene->statementText());
			}
		};
		connect(row->commutesSwitch, &QAbstractButton::toggled, this, [claim](bool on) {
			claim(&Node::setCommutesInComponent, on, true);
		});
		connect(row->rowsSwitch, &QAbstractButton::toggled, this, [claim](bool on) {
			claim(&Node::setRowsExactInComponent, on, false);
		});
		connect(row->columnsSwitch, &QAbstractButton::toggled, this, [claim](bool on) {
			claim(&Node::setColumnsExactInComponent, on, false);
		});

		item->setSizeHint(row->sizeHint());
		m_components->setItemWidget(item, row);
	}
}

void PropertiesDock::lightUp(const QList<Node*>& objects, const QList<Arrow*>& arrows, bool on)
{
	for (Node* node : objects)
		if (node != nullptr)
			node->setHighlight(on);
	for (Arrow* arrow : arrows)
		if (arrow != nullptr)
			arrow->setHighlight(on);
}
