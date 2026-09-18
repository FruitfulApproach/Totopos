#include "widget/PropertiesDock.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QGroupBox>
#include <QSpinBox>
#include <QPushButton>
#include <QScrollArea>
#include <QComboBox>
#include <QCheckBox>
#include <QSignalBlocker>

#include <QListWidget>
#include <QEvent>
#include <functional>

#include "art/DiagramScene.h"
#include "widget/SketchView.h"
#include "art/Object.h"
#include "art/Category.h"
#include "art/Arrow.h"
#include "art/Functor.h"
#include <QMouseEvent>
<<<<<<< HEAD:DiagramDetective/DiagramDetective/widget/PropertiesDock.cpp
#include "widget/ToggleSwitch.h"
#include "core/Notation.h"
#include "core/props/MapsElements.h"
#include "core/history/SceneHistory.h"
#include "core/history/Mementos.h"
=======
#include "ToggleSwitch.h"
#include "Notation.h"
#include "Emoji.h"
#include "props/MapsElements.h"
#include "NodeKind.h"
#include "AtomicElement.h"
#include "CategoryDialog.h"
#include "Tutor.h"
#include "AppSettings.h"
#include "history/SceneHistory.h"
#include "history/Mementos.h"
>>>>>>> 3e9da9ce39d6dc74c5a0385266cfd9f7c2eeaba9:DiagramDetective/DiagramDetective/PropertiesDock.cpp

namespace
{
	// the last entry of the category list: it opens the dialog instead of
	// naming a category that already exists
	const QString kCustom = QStringLiteral("Custom...");

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

	// What this node IS. Not a setting on the node: picking another entry
	// builds the node afresh as that kind and hands everything over - the
	// name, where it sits, what is drawn inside it, the arrows that end on
	// it - because what a node can DO is decided by which kind it is.
	m_type = new QComboBox(m_nodeBox);
	m_type->setToolTip("What this node is. An object is an object of the category it is drawn in; an "
	                   "element is a member of the node it is drawn in and holds nothing; a category is a "
	                   "world of its own; a subcategory is a PART of the category it is drawn in.");
	connect(m_type, &QComboBox::currentIndexChanged, this, [this](int index) {
		if (m_updating || index < 0)
			return;
		// The node this combo is about is taken out of the scene by the
		// change, and this runs from that very combo's signal: let the signal
		// finish first.
		const QString kind = m_type->itemData(index).toString();
		QMetaObject::invokeMethod(this, [this, kind] { applyNodeKind(kind); }, Qt::QueuedConnection);
	});
	nodeForm->addRow("Type", m_type);

	m_typeHint = new QLabel(m_nodeBox);
	m_typeHint->setWordWrap(true);
	m_typeHint->setEnabled(false);
	nodeForm->addRow(QString(), m_typeHint);

	m_exists = new ToggleSwitch(m_nodeBox);
	m_exists->setToolTip("Draw it dotted and read it as the part that is claimed to EXIST.");
	connect(m_exists, &QAbstractButton::toggled, this, [this](bool on) { applyExistsSuch(on); });
	nodeForm->addRow("Exists such", m_exists);
	m_deleteMark = new ToggleSwitch(m_nodeBox);
	m_deleteMark->setToolTip("Cross it out in red. Read as a rule, the diagram still has to FIND this - "
	                         "but where the rule is applied, this is what gets taken out.");
	connect(m_deleteMark, &QAbstractButton::toggled, this, [this](bool on) { applyDeleteMark(on); });
	nodeForm->addRow("Delete on apply", m_deleteMark);
	layout->addWidget(m_nodeBox);

	// ---- what a CATEGORY is, and what the diagram drawn in it claims.
	// These used to live only on the panel over the canvas, where they could
	// only ever be about the whole picture. They belong to a category, and
	// the canvas is simply the outermost one - so a subcategory of R-Mod
	// drawn four levels down is asked exactly the same questions.
	m_categoryBox = new QGroupBox("Category", body);
	auto* categoryForm = new QFormLayout(m_categoryBox);

	m_categoryKind = new QComboBox(m_categoryBox);
	m_categoryKind->addItems(Category::builtInNames());
	m_categoryKind->addItem(kCustom);   // last: defines a new one through the dialog
	m_categoryKind->setToolTip("Which category this is. A built-in knows what its objects and its arrows "
	                           "are, so choosing R-Mod here makes what you place inside it an R-module and "
	                           "what you draw between them an R-linear map.");
	connect(m_categoryKind, &QComboBox::currentIndexChanged, this, [this](int index) {
		if (m_updating || index < 0)
			return;
		const QString name = m_categoryKind->itemText(index);
		QMetaObject::invokeMethod(this, [this, name] { applyCategoryKind(name); }, Qt::QueuedConnection);
	});
	categoryForm->addRow("Category", m_categoryKind);

	m_subcategoryHint = new QLabel(m_categoryBox);
	m_subcategoryHint->setWordWrap(true);
	m_subcategoryHint->setEnabled(false);
	categoryForm->addRow(QString(), m_subcategoryHint);

	// tutor mode: guided interactions coach with remarks and an arrow; off,
	// they run quietly. App-wide, and shown here because it is what decides
	// how everything you do in this category behaves.
	m_tutor = new QCheckBox("Tutor mode", m_categoryBox);
	m_tutor->setToolTip("Guided actions (Define a product...) explain each step with remarks and an arrow. "
	                    "Untick to run them quietly. This is the same setting as the one on the sketch "
	                    "panel and in Tools > Settings.");
	connect(m_tutor, &QCheckBox::toggled, this, [this](bool on) {
		if (m_updating) return;
		AppSettings::instance().setValue(AppSettings::TutorEnabled, on);
		AppSettings::instance().apply();
	});
	categoryForm->addRow(QString(), m_tutor);

	m_commutesLabel = new QLabel(m_categoryBox);
	m_commutes = new ToggleSwitch(m_categoryBox);
	connect(m_commutes, &QAbstractButton::toggled, this, [this](bool on) {
		refreshCommutesLabel(on);
		if (m_updating) return;
		if (Category* category = pageCategory())
			category->setCommutes(on);
	});
	categoryForm->addRow(m_commutesLabel, m_commutes);
	refreshCommutesLabel(false);

	m_statementKind = new QComboBox(m_categoryBox);
	m_statementKind->addItems(DiagramScene::kindNames());
	m_statementKind->setToolTip("The picture says the same thing either way; what changes is what saying "
	                            "it amounts to. An axiom is granted, a definition names something, a "
	                            "conjecture is neither, and a theorem owes a proof.");
	connect(m_statementKind, &QComboBox::currentIndexChanged, this, [this](int index) {
		if (m_updating || index < 0) return;
		if (Category* category = pageCategory())
			category->setStatementKind(index);
	});
	categoryForm->addRow("This is", m_statementKind);

	m_statementName = new QLineEdit(m_categoryBox);
	m_statementName->setPlaceholderText("Additive identity exists");
	m_statementName->setToolTip("What to call it, so it can be referred to from elsewhere.");
	connect(m_statementName, &QLineEdit::editingFinished, this, [this] {
		if (m_updating) return;
		if (Category* category = pageCategory())
			category->setStatementName(m_statementName->text().trimmed());
	});
	categoryForm->addRow("Called", m_statementName);

	m_mode = new QLabel("Let", m_categoryBox);
	QFont modeFont = m_mode->font();
	modeFont.setBold(true);
	m_mode->setFont(modeFont);
	m_mode->setToolTip("A let is what you are given. A chase forces anything you add into the hypotheses "
	                   "of the statement. The chase belongs to the whole diagram, not to one category.");
	categoryForm->addRow("Mode", m_mode);

	m_chase = new QPushButton("Start diagram chase", m_categoryBox);
	connect(m_chase, &QPushButton::clicked, this, [this] {
		if (m_scene != nullptr)
			m_scene->toggleChase();
	});
	categoryForm->addRow(QString(), m_chase);
	layout->addWidget(m_categoryBox);

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

	// ---- what an arrow is asserted to be
	m_arrowBox = new QGroupBox("Arrow", body);
	auto* arrowForm = new QFormLayout(m_arrowBox);
	const QString to = Emoji::to();
	const QString ring = Emoji::compose();
	m_monic = new ToggleSwitch(m_arrowBox);
	m_monic->setToolTip(QString("Cancellable on the left: for g, h : Z %1 X, f%2g = f%2h implies g = h. "
	                            "Drawn with a hooked tail, the way an inclusion usually is.").arg(to, ring));
	connect(m_monic, &QAbstractButton::toggled, this, [this](bool on) {
		if (m_updating) return;
		if (Arrow* arrow = soleArrow()) arrow->setMonicRecorded(on);
	});
	arrowForm->addRow("Monomorphism", m_monic);
	m_epic = new ToggleSwitch(m_arrowBox);
	m_epic->setToolTip(QString("Cancellable on the right: for g, h : Y %1 Z, g%2f = h%2f implies g = h. "
	                           "Drawn with a doubled head, the way a quotient usually is.").arg(to, ring));
	connect(m_epic, &QAbstractButton::toggled, this, [this](bool on) {
		if (m_updating) return;
		if (Arrow* arrow = soleArrow()) arrow->setEpicRecorded(on);
	});
	arrowForm->addRow("Epimorphism", m_epic);
	layout->addWidget(m_arrowBox);

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
		// the chase is the whole diagram's mode, and the page says which it is in
		connect(m_scene, &DiagramScene::chasingChanged, this, &PropertiesDock::refresh);
		connect(m_scene, &DiagramScene::commutesChanged, this, &PropertiesDock::refresh);
		connect(m_scene, &DiagramScene::ambientCategoryChanged, this, &PropertiesDock::refresh);
	}
	// Tutor mode is app-wide: Tools > Settings and the sketch panel set the
	// same thing, and the tick here must not be left saying otherwise.
	connect(&AppSettings::instance(), &AppSettings::changed, this, &PropertiesDock::refresh, Qt::UniqueConnection);
	refresh();
}

Category* PropertiesDock::soleCategory() const
{
	const QList<Node*> nodes = selection();
	if (nodes.size() != 1)
		return nullptr;
	return dynamic_cast<Category*>(nodes.first());
}

Category* PropertiesDock::pageCategory() const
{
	// One category picked out is that category. Nothing picked out is a
	// question about the whole picture, and the whole picture is the ambient
	// category - so the same page answers both.
	if (Category* sole = soleCategory())
		return sole;
	if (m_scene == nullptr)
		return nullptr;
	return selection().isEmpty() ? m_scene->ambientCategory() : nullptr;
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

Arrow* PropertiesDock::soleArrow() const
{
	const QList<Node*> nodes = selection();
	if (nodes.size() != 1)
		return nullptr;
	return dynamic_cast<Arrow*>(nodes.first());
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
		m_arrowBox->hide();
		m_mappingBox->hide();
		m_insideBox->hide();

		// nothing picked out is a question about the whole picture, and the
		// whole picture is the category everything is drawn in
		refreshCategoryBox(ambient);
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
		m_arrowBox->hide();
		m_mappingBox->hide();
		m_insideBox->hide();
		m_categoryBox->hide();
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

		// What it could be instead. The list is built afresh each time: a
		// subcategory is only on offer inside a category, and the entry has to
		// name the category it would be a subcategory OF.
		const QList<NodeKind::Choice> choices = NodeKind::choices(node);
		const QString current = NodeKind::of(node);
		m_type->clear();
		for (const NodeKind::Choice& choice : choices)
		{
			m_type->addItem(choice.label, choice.id);
			m_type->setItemData(m_type->count() - 1, choice.tip, Qt::ToolTipRole);
		}
		// A kind the list does not offer - the canvas, or a node whose kind
		// has gone - still has to show as what it is rather than as the first
		// entry, which would be a change nobody asked for.
		int index = m_type->findData(current);
		if (index < 0 && !choices.isEmpty())
		{
			m_type->addItem(NodeKind::label(current), current);
			index = m_type->count() - 1;
		}
		m_type->setCurrentIndex(index);

		// the canvas is not one of the things drawn on it, and an arrow is a
		// different question with its own page
		const bool retypeable = !choices.isEmpty()
			&& (m_scene == nullptr || m_scene->ambientCategory() != node);
		m_type->setVisible(retypeable);
		m_typeHint->setVisible(retypeable);
		if (retypeable && index >= 0 && index < choices.size())
			m_typeHint->setText(choices.at(index).tip);
		else
			m_typeHint->clear();
	}
	else
	{
		m_header->setText(QString("%1 items selected").arg(nodes.size()));
		m_id->setEnabled(false);
		m_id->setText(QString());
		m_type->setVisible(false);
		m_typeHint->setVisible(false);
	}

	m_nodeBox->show();
	bool allExist = true;
	for (Node* node : nodes)
		allExist = allExist && node->existsSuch();
	m_exists->setChecked(allExist);
	bool allStruck = true;
	for (Node* node : nodes)
		allStruck = allStruck && node->markedForDeletion();
	m_deleteMark->setChecked(allStruck);

	m_objectBox->setVisible(!objects.isEmpty());
	if (!objects.isEmpty())
	{
		m_radius->setValue(int(objects.first()->cornerRadius() + 0.5));
		m_objectBox->setTitle(objects.size() == 1 ? QStringLiteral("Object")
		                                          : QString("Objects (%1)").arg(objects.size()));
	}

	Arrow* soleA = soleArrow();
	m_arrowBox->setVisible(soleA != nullptr);
	if (soleA != nullptr)
	{
		m_monic->setChecked(soleA->isMonic());
		m_epic->setChecked(soleA->isEpic());
	}

	refreshCategoryBox(soleCategory());

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
	// as typed: the subscripts are put on when the label is drawn, not stored
	const QString written = id;
	if (nodes.size() != 1 || nodes.first()->id() == written)
		return;
	Node* node = nodes.first();
	const QString before = node->id();
	node->setId(written);
	node->bindLabelReferences();
	if (m_scene != nullptr)
		m_scene->history()->record(new Renamed(QString("Renamed %1 to %2").arg(before, written), node, before, written));
}

void PropertiesDock::applyNodeKind(const QString& kindId)
{
	if (m_updating || kindId.isEmpty())
		return;
	const QList<Node*> nodes = selection();
	if (nodes.size() != 1)
		return;
	Node* node = nodes.first();
	if (NodeKind::of(node) == kindId)
		return;

	Node* fresh = NodeKind::retype(node, kindId);
	if (fresh != nullptr && fresh != node && m_scene != nullptr)
	{
		// the node the page was about has been put away; the page follows the
		// one that has taken its place
		m_scene->clearSelection();
		fresh->setSelected(true);
	}
	refresh();
}

void PropertiesDock::applyCategoryKind(const QString& name)
{
	if (m_updating || name.isEmpty() || m_scene == nullptr)
		return;
	Category* category = pageCategory();
	if (category == nullptr)
		return;

	// Custom... is not a category: it is the dialog that defines one
	QString kindName = name;
	QStringList customProps;
	bool custom = false;
	if (name == kCustom)
	{
		CategoryDialog dialog(window());
		if (dialog.exec() != QDialog::Accepted || dialog.name().isEmpty())
		{
			refresh();   // put the combo back to what it was, quietly
			return;
		}
		kindName = dialog.name();
		customProps = dialog.properties();
		custom = Category::createBuiltIn(kindName) == nullptr;
	}

	// The canvas is not one of the things drawn on it, so it is not swapped
	// for another node: the scene changes what it is drawn in.
	if (category->isAmbient())
	{
		m_scene->setAmbientCategory(kindName);
		if (custom)
			if (Category* ambient = m_scene->ambientCategory())
				ambient->setProperties(customProps);
		refresh();
		return;
	}

	const bool wasSubcategory = category->isSubcategory();
	Node* fresh = NodeKind::retype(category, custom ? NodeKind::category() : NodeKind::builtIn(kindName));
	if (auto* freshCat = dynamic_cast<Category*>(fresh))
	{
		if (custom)
		{
			freshCat->setId(kindName);
			freshCat->setProperties(customProps);
		}
		// changing WHICH category it is does not stop it being a part of the
		// one it is drawn in
		if (wasSubcategory)
			freshCat->setSubcategory(true);
		if (fresh != category)
		{
			m_scene->clearSelection();
			fresh->setSelected(true);
		}
	}
	refresh();
}

void PropertiesDock::refreshCommutesLabel(bool commutes)
{
	if (m_commutesLabel == nullptr)
		return;

	// The off state is NOT the claim that the diagram fails to commute. It is
	// the absence of a claim: the paths may or may not agree, and the diagram
	// says nothing about it either way.
	const QString name = commutes ? QStringLiteral("Commutative") : QStringLiteral("Non-commutative");
	const QString tip = commutes
		? QStringLiteral("Commutative: every pair of paths with the same two ends is asserted to be the "
		                 "same arrow. The statement then reads \"... such that the diagram commutes\", the "
		                 "Equations dock lists what that says, and a cycle becomes an error - a ring gives "
		                 "endlessly many paths, which cannot be read this way.")
		: QStringLiteral("Non-commutative means NOT NECESSARILY COMMUTATIVE. It is not the claim that the "
		                 "diagram fails to commute: it is the absence of any claim. Two paths with the same "
		                 "ends may or may not be the same arrow, nothing is asserted either way, and cycles "
		                 "are perfectly all right.");

	m_commutesLabel->setText(name);
	m_commutesLabel->setToolTip(tip);
	if (m_commutes != nullptr)
		m_commutes->setToolTip(tip);
}

void PropertiesDock::refreshCategoryBox(Category* category)
{
	m_categoryBox->setVisible(category != nullptr);
	if (category == nullptr)
		return;

	const bool ambient = category->isAmbient();
	m_categoryBox->setTitle(ambient
		? QString("The whole diagram, drawn in %1").arg(category->id())
		: QString("The category %1").arg(category->id()));

	// which category it is. A built-in answers with its own name; one the
	// user defined is not in the list, so its label goes in before Custom...
	{
		const QSignalBlocker block(m_categoryKind);
		QString kind = category->builtInName();
		if (kind.isEmpty())
			kind = category->id();
		int index = m_categoryKind->findText(kind);
		if (index < 0)
		{
			index = m_categoryKind->count() - 1;   // before Custom...
			m_categoryKind->insertItem(index, kind);
		}
		m_categoryKind->setCurrentIndex(index);
	}

	if (category->isSubcategory())
	{
		Category* home = category->ambient();
		m_subcategoryHint->setText(home != nullptr
			? QString("A subcategory of %1. Its objects are %2s of %1 and its arrows are %3s of %1.")
				.arg(home->id(), home->objectName(), home->morphismName())
			: QStringLiteral("A subcategory."));
		m_subcategoryHint->show();
	}
	else
	{
		m_subcategoryHint->setText(QString("Its objects are %1s and its arrows are %2s.")
			.arg(category->objectName(), category->morphismName()));
		m_subcategoryHint->show();
	}

	m_tutor->setChecked(Tutor::isEnabled());

	m_commutes->setChecked(category->commutes());
	refreshCommutesLabel(category->commutes());

	m_statementKind->setCurrentIndex(category->statementKind());
	if (m_statementName->text() != category->statementName())
		m_statementName->setText(category->statementName());

	// the chase is the whole diagram's mode, so it reads the same on every
	// category's page
	const bool chasing = m_scene != nullptr && m_scene->isChasing();
	m_mode->setText(chasing ? "Chasing" : "Let");
	m_chase->setText(chasing ? "End the chase" : "Start diagram chase");
	m_chase->setToolTip(chasing
		? "Go back to a let: the diagram is what you are given again, and adding to it assumes nothing."
		: "Start the diagram chase (Ctrl+Shift+Enter): from then on anything you draw is forced into "
		  "the hypotheses of the statement.");
}

void PropertiesDock::applyExistsSuch(bool on)
{
	if (m_updating)
		return;
	for (Node* node : selection())
		node->setExistsSuchRecorded(on);
}

void PropertiesDock::applyDeleteMark(bool on)
{
	if (m_updating)
		return;
	for (Node* node : selection())
		node->setDeleteMarkRecorded(on);
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
