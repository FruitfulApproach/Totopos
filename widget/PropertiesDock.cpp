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
#include <QColorDialog>
#include <QHBoxLayout>
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
#include "widget/ToggleSwitch.h"
#include "core/Notation.h"
#include "core/Emoji.h"
#include "core/AppSettings.h"
#include "core/NodeKind.h"
#include "core/props/MapsElements.h"
#include "art/AtomicElement.h"
#include "dialog/CategoryDialog.h"
#include "tutor/Tutor.h"
#include "core/history/SceneHistory.h"
#include "core/history/Mementos.h"

namespace
{
	// the last entry of the category list: it opens the dialog instead of
	// naming a category that already exists
	const QString kCustom = QStringLiteral("Custom...");

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

	// HOW IT IS DRAWN. Two buttons that open a colour dialog, each showing the
	// colour it would change as its own swatch, and each able to say "none" -
	// which is not the same as white: a node with no fill is transparent, and
	// one with no border has none drawn at all.
	auto* colours = new QWidget(m_nodeBox);
	auto* colourRow = new QHBoxLayout(colours);
	colourRow->setContentsMargins(0, 0, 0, 0);
	m_fillColour = new QPushButton("Fill", colours);
	m_fillColour->setToolTip("The colour inside. A colour chosen by hand is always drawn, even on a node "
	                         "that holds nothing and would otherwise be just its label.");
	connect(m_fillColour, &QAbstractButton::clicked, this, [this] { applyColour(true); });
	colourRow->addWidget(m_fillColour);
	m_borderColour = new QPushButton("Border", colours);
	m_borderColour->setToolTip("The colour of the frame round it.");
	connect(m_borderColour, &QAbstractButton::clicked, this, [this] { applyColour(false); });
	colourRow->addWidget(m_borderColour);
	nodeForm->addRow("Appearance", colours);
	layout->addWidget(m_nodeBox);

	// ---- what a CATEGORY is, and what the diagram drawn in it claims.
	// These used to live only on the panel over the canvas, where they could
	// only ever be about the whole picture. They belong to a category, and
	// the canvas is simply the outermost one - so a subcategory of R-Mod
	// drawn four levels down is asked exactly the same questions.
	m_categoryBox = new QGroupBox("Diagram", body);
	auto* categoryForm = new QFormLayout(m_categoryBox);

	// WHICH CATEGORY THIS IS: SAID, NOT OFFERED.
	//
	// It used to be a dropdown. But a category settles the moment anything is
	// drawn in it - everything inside is an object or an arrow OF it and would
	// mean something else in another - so for all but the first moment of a
	// diagram's life the control was a disabled combo wearing a padlock: a
	// menu of choices none of which could be taken. That reads as something
	// broken rather than as something decided.
	//
	// So it is a sentence now. The combo is kept below, commented out, because
	// the question it asked is a real one and may want asking somewhere it can
	// still be answered.
	m_categoryName = new QLabel(m_categoryBox);
	m_categoryName->setWordWrap(true);
	categoryForm->addRow(m_categoryName);

	/*
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
	*/

	// THE COLOUR OF THE PAPER, asked for the way a node's fill is.
	//
	// Only on the canvas's own page: every other category is a node, and a
	// node's colour is the Fill chip above. The canvas is not a node - it has
	// no fill to set - and the thing that answers to "what colour is this" for
	// the canvas is the scene's background.
	m_background = new QPushButton("Background", m_categoryBox);
	m_background->setToolTip("The colour of the paper everything is drawn on. Cleared with \"None\" in "
	                         "the dialog, which gives it back the colour the window is drawn in.");
	connect(m_background, &QAbstractButton::clicked, this, &PropertiesDock::applyBackgroundColour);
	categoryForm->addRow("Appearance", m_background);

	m_subcategoryHint = new QLabel(m_categoryBox);
	m_subcategoryHint->setWordWrap(true);
	m_subcategoryHint->setEnabled(false);
	categoryForm->addRow(QString(), m_subcategoryHint);

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

	// ---- AND WHAT THE DIAGRAM DRAWN IN IT CLAIMS, in the same box.
	//
	// These were a second group of their own, headed "The diagram in BigCat"
	// while the one above it said "The whole diagram, drawn in BigCat" - two
	// frames, two headings, one subject. A category and the diagram drawn in
	// it are not two things to be asked about separately, so they are one
	// group now, and the rows below appear only once there IS a diagram.
	// Commuting belongs beside the two exactness switches, and for the same
	// reason: all three are claims about a DIAGRAM, and a diagram is what a
	// node holds. It used to sit in the category box and speak for the whole
	// page, which left a nested category with no way to say that the diagram
	// drawn inside IT commutes. Each parent now answers for its own.
	m_commutesLabel = new QLabel(m_categoryBox);
	m_commutes = new ToggleSwitch(m_categoryBox);
	connect(m_commutes, &QAbstractButton::toggled, this, [this](bool on) {
		refreshCommutesLabel(on);
		if (m_updating) return;
		// pageCategory, not the selection: with nothing selected the page is
		// about the ambient category, and the switch has to write there too
		if (Category* home = pageCategory())
			home->setCommutes(on);
	});
	categoryForm->addRow(m_commutesLabel, m_commutes);
	refreshCommutesLabel(false);
	m_rowsExact = new ToggleSwitch(m_categoryBox);
	m_rowsExact->setToolTip("Every row of the diagram drawn in here is an exact sequence: at each object "
	                        "along it, the image of the arrow coming in is the kernel of the arrow going out.");
	connect(m_rowsExact, &QAbstractButton::toggled, this, [this](bool on) {
		if (m_updating) return;
		if (Category* home = pageCategory()) home->setRowsExactRecorded(on);
	});
	categoryForm->addRow("Rows exact", m_rowsExact);
	m_columnsExact = new ToggleSwitch(m_categoryBox);
	m_columnsExact->setToolTip("The same, down each column. Rows and columns are claimed separately.");
	connect(m_columnsExact, &QAbstractButton::toggled, this, [this](bool on) {
		if (m_updating) return;
		if (Category* home = pageCategory()) home->setColumnsExactRecorded(on);
	});
	categoryForm->addRow("Columns exact", m_columnsExact);
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

	// The narrower claim, under the one it narrows. An inclusion IS a
	// monomorphism, so turning this on turns that on with it and greys it:
	// there is no such thing as an inclusion that is not monic, and a switch
	// that could say otherwise would only invite it.
	m_inclusion = new ToggleSwitch(m_arrowBox);
	m_inclusion->setToolTip("Takes a part of something into the whole of it, carrying x to x - "
	                        "the inclusion of a submodule, a subgroup, a subspace. Every inclusion "
	                        "is a monomorphism; this one is drawn with a hooked tail.");
	connect(m_inclusion, &QAbstractButton::toggled, this, [this](bool on) {
		if (m_updating) return;
		if (Arrow* arrow = soleArrow()) arrow->setInclusionRecorded(on);
	});
	arrowForm->addRow("Inclusion", m_inclusion);

	m_epic = new ToggleSwitch(m_arrowBox);
	m_epic->setToolTip(QString("Cancellable on the right: for g, h : Y %1 Z, g%2f = h%2f implies g = h. "
	                           "Drawn with a doubled head, the way a quotient usually is.").arg(to, ring));
	connect(m_epic, &QAbstractButton::toggled, this, [this](bool on) {
		if (m_updating) return;
		if (Arrow* arrow = soleArrow()) arrow->setEpicRecorded(on);
	});
	arrowForm->addRow("Epimorphism", m_epic);

	// WHAT KIND OF ARROW IT IS DRAWN AS.
	//
	// Overlapping with the two switches above and deliberately not tied to
	// them: those are claims about cancellation, this is the mark on the
	// line. They are free to disagree - a diagram may draw a hooked tail
	// without asserting anything, or assert monic without drawing a barb -
	// and forcing them into step would take away a distinction people make.
	m_arrowStyle = new QComboBox(m_arrowBox);
	for (Arrow::Style option : Arrow::allStyles())
		m_arrowStyle->addItem(Arrow::styleName(option), int(option));
	m_arrowStyle->setToolTip("The mark on the line: a hooked tail for an inclusion, a barbed tail for a "
	                         "monomorphism, a second head for an epimorphism, a tilde for an iso.");
	connect(m_arrowStyle, &QComboBox::currentIndexChanged, this, [this](int index) {
		if (m_updating || index < 0)
			return;
		if (Arrow* arrow = soleArrow())
			arrow->setStyleRecorded(Arrow::Style(m_arrowStyle->itemData(index).toInt()));
	});
	arrowForm->addRow("Style", m_arrowStyle);

	// THE SHAPE OF THE LINE. Adding a bend needs a point to put it at, and the
	// panel has none - that one stays on the right-click menu of the line
	// itself, where the cursor says where. Taking them all out again needs no
	// point at all, so it belongs here.
	m_straighten = new QPushButton("Straighten", m_arrowBox);
	m_straighten->setToolTip("Take every bend out of the line. Add a bend by right-clicking the line "
	                         "where you want it, which is the one thing here that needs a place.");
	connect(m_straighten, &QAbstractButton::clicked, this, [this] {
		if (Arrow* arrow = soleArrow())
			arrow->straightenRecorded();
	});
	arrowForm->addRow("Shape", m_straighten);
	layout->addWidget(m_arrowBox);

	// ---- what an arrow does to what is drawn in its domain
	m_mappingBox = new QGroupBox("Mapping", body);
	auto* mappingForm = new QFormLayout(m_mappingBox);

	m_mappingHint = new QLabel(m_mappingBox);
	m_mappingHint->setWordWrap(true);
	m_mappingHint->setEnabled(false);
	mappingForm->addRow(m_mappingHint);

	// TWO SWITCHES, not eight and not one. Carrying object moves, bend points
	// and label placements across - each way round - were four toggles, and
	// nobody wanted half of a mirror: those are one answer. But whether there
	// is an image at all is a different question from whether the two sides
	// travel together, and folding it in meant that freezing the arrangement
	// put the whole image away, which read as a bug because it was one.
	m_live = new ToggleSwitch(m_mappingBox);
	connect(m_live, &QAbstractButton::toggled, this, [this](bool on) {
		if (m_updating) return;
		if (MapsElements* maps = soleMapping()) maps->setLive(on);
	});
	mappingForm->addRow("Show the image, kept live", m_live);

	m_mirror = new ToggleSwitch(m_mappingBox);
	connect(m_mirror, &QAbstractButton::toggled, this, [this](bool on) {
		if (m_updating) return;
		if (MapsElements* maps = soleMapping()) maps->setMirrorsGeometry(on);
	});
	mappingForm->addRow("Mirror geometry changes", m_mirror);

	// Contravariance is NOT part of the mirror: which way the image arrows run
	// is what the mapping MEANS, not how its two sides are kept in step.
	m_contravariant = new ToggleSwitch(m_mappingBox);
	connect(m_contravariant, &QAbstractButton::toggled, this, [this](bool on) {
		if (m_updating) return;
		if (MapsElements* maps = soleMapping()) maps->setContravariant(on);
	});
	mappingForm->addRow("Contravariant", m_contravariant);

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

		// nothing picked out is a question about the whole picture, and the
		// whole picture is the category everything is drawn in - which is a
		// parent node like any other, and answers for the diagram it holds
		refreshCategoryBox(ambient);
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
		m_categoryBox->hide();
		m_updating = false;
		return;
	}
	m_hint->hide();

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

	// each colour button wears the colour it would change, so the panel shows
	// what is there rather than only offering to alter it
	{
		Node* first = nodes.first();
		const QColor fill = first->fill().style() == Qt::NoBrush ? QColor() : first->fill().color();
		const QColor border = first->border().style() == Qt::NoPen ? QColor() : first->border().color();
		m_fillColour->setText(fill.isValid() ? QStringLiteral("Fill") : QStringLiteral("Fill: none"));
		m_borderColour->setText(border.isValid() ? QStringLiteral("Border") : QStringLiteral("Border: none"));
		m_fillColour->setStyleSheet(colourSwatch(fill));
		m_borderColour->setStyleSheet(colourSwatch(border));
	}

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
		m_inclusion->setChecked(soleA->isInclusion());
		// an inclusion is monic whatever the switch says, so the switch stops
		// being something that can be argued with
		m_monic->setEnabled(!soleA->isInclusion());
		m_epic->setChecked(soleA->isEpic());
		if (const int index = m_arrowStyle->findData(int(soleA->style())); index >= 0)
			m_arrowStyle->setCurrentIndex(index);
		// nothing to straighten on a line that is already straight
		m_straighten->setEnabled(!soleA->bends().isEmpty());
	}

	refreshCategoryBox(soleCategory());

	MapsElements* maps = soleMapping();
	m_mappingBox->setVisible(maps != nullptr);
	if (maps != nullptr)
	{
		const QString dom = maps->domain()->id();
		const QString cod = maps->codomain()->id();
		const QString to = QString(QChar(0x2192));
		m_mappingBox->setTitle(QString("Mapping  %1 %2 %3").arg(dom, to, cod));
		m_mappingHint->setText(QString("What is drawn in %1 appears in %2.").arg(dom, cod));
		m_live->setChecked(maps->isLive());
		m_live->setToolTip(QString(
			"Draw the image of %1 in %2 and keep it there: it follows whatever is drawn or "
			"deleted in %1, and is relabelled when %1 is.\n\n"
			"Off, the image is put away - hidden, not given up: whatever is drawn inside it comes "
			"back untouched when this goes on again.").arg(dom, cod));
		m_mirror->setChecked(maps->mirrorsGeometry());
		m_mirror->setToolTip(QString(
			"Keep the ARRANGEMENT of %1 and %2 in step, both ways round.\n\n"
			"Moving an object, bending an arrow, or dragging a label clear of its node moves the "
			"matching one on the far side BY THE SAME AMOUNT (%1 %3 %2, and %2 %3 %1) - each side "
			"keeps the arrangement you gave it and simply travels with the other. Whichever one "
			"you drag leads, and the other follows without answering back.\n\n"
			"Off, each side is arranged on its own. Nothing appears or disappears either way: what "
			"is drawn in %1 still appears in %2 for as long as the image is live.").arg(dom, cod, to));
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
	}

	m_updating = false;
}

void PropertiesDock::applyId(const QString& raw)
{
	if (m_updating)
		return;
	const QList<Node*> nodes = selection();
	// the same correction the label editor does, so it makes no difference
	// which of the two the name was typed into
	const QString written = Notation::autoCorrect(raw);
	if (written != raw && m_id != nullptr)
		m_id->setText(written);   // and show what was actually taken
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

	m_categoryBox->setTitle(QString("Diagram in %1").arg(category->id()));

	// WHICH CATEGORY IT IS, in a sentence. A built-in answers with its own
	// name; one the user defined has only the name it was given, and the two
	// are the same thing to say out loud. The name it is DRAWN under goes in
	// too when it differs - a built-in R-Mod renamed to C is still R-Mod, and
	// that is worth being able to see.
	{
		QString kind = category->builtInName();
		if (kind.isEmpty())
			kind = category->id();
		m_categoryName->setText(kind == category->id()
			? QString("Diagram in category \"%1\".").arg(kind)
			: QString("Diagram in category \"%1\", drawn as %2.").arg(kind, category->id()));
		m_categoryName->setToolTip(QString("Everything drawn in here is an object or an arrow of %1. "
		                                   "That settles as soon as anything is placed: what is drawn in "
		                                   "one category would mean something else in another.").arg(kind));
	}

	// The paper is the canvas's, so the chip is only on the canvas's page.
	const bool isCanvas = category->isAmbient() && m_scene != nullptr;
	m_background->setVisible(isCanvas);
	if (auto* row = m_categoryBox->layout(); row != nullptr)
		if (auto* form = qobject_cast<QFormLayout*>(row))
			if (QWidget* label = form->labelForField(m_background); label != nullptr)
				label->setVisible(isCanvas);
	if (isCanvas)
		m_background->setStyleSheet(colourSwatch(m_scene->background()));

	/*
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

		// SETTLED ONCE ANYTHING IS DRAWN IN IT.
		//
		// Everything inside a category is an object or an arrow OF it, and
		// would mean something else - or nothing - in another. An R-module is
		// not a group is not a topological space, so the kind can be chosen
		// while the category is empty and not afterwards.
		const bool settled = category->holdsAnything();
		m_categoryKind->setEnabled(!settled);
		const QIcon lock = settled ? Emoji::icon(Emoji::locked()) : QIcon();
		for (int i = 0; i < m_categoryKind->count(); ++i)
			m_categoryKind->setItemIcon(i, i == index ? lock : QIcon());
		m_categoryKind->setToolTip(settled
			? QString("%1 already holds something, so what kind of category it is has settled: "
			          "what is drawn in it would mean something else in another kind. Empty it, "
			          "or start again, to choose differently.").arg(category->id())
			: QStringLiteral("Which category this is. It can be chosen while the category is "
			                 "still empty."));
	}
	*/

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

	m_statementKind->setCurrentIndex(category->statementKind());

	// WHAT THE DIAGRAM IN HERE CLAIMS - only once there is one.
	//
	// Commuting, and the two exactness claims, are about a DIAGRAM, and an
	// empty category holds none: there is nothing there to commute. Exactness
	// asks about images and kernels, which is not a question in Set or Top, so
	// those two go as well where the category cannot answer them.
	const bool drawn = category->holdsAnything();
	const bool exact = drawn && category->exactnessDefined();
	m_commutes->setChecked(category->commutes());
	refreshCommutesLabel(category->commutes());
	m_rowsExact->setChecked(category->rowsExact());
	m_columnsExact->setChecked(category->columnsExact());
	if (auto* form = qobject_cast<QFormLayout*>(m_categoryBox->layout()))
	{
		form->setRowVisible(m_commutes, drawn);
		form->setRowVisible(m_rowsExact, exact);
		form->setRowVisible(m_columnsExact, exact);
	}
}

QString PropertiesDock::colourSwatch(const QColor& colour)
{
	// A chip wears the colour it would change, so the page shows what is set
	// without anybody having to open the dialog to find out.
	if (!colour.isValid())
		return QString();
	// black on a light colour, white on a dark one, so the word on the button
	// can still be read whatever it is sitting on
	const bool dark = colour.lightness() < 128 && colour.alpha() > 96;
	return QString("background-color: rgba(%1,%2,%3,%4); color: %5;")
		.arg(colour.red()).arg(colour.green()).arg(colour.blue()).arg(colour.alpha())
		.arg(dark ? "white" : "black");
}

void PropertiesDock::applyBackgroundColour()
{
	if (m_updating || m_scene == nullptr)
		return;

	// Opens on the colour already set, so nudging a shade starts from the
	// shade rather than from black. No alpha: there is nothing behind the
	// paper for it to be see-through against.
	const QColor current = m_scene->background();
	QColorDialog dialog(current.isValid() ? current : QColor(Qt::white), this);
	dialog.setOption(QColorDialog::ShowAlphaChannel, false);
	dialog.setWindowTitle(QStringLiteral("Background"));
	if (dialog.exec() != QDialog::Accepted)
		return;
	m_scene->setBackground(dialog.currentColor());
	refresh();
}

void PropertiesDock::applyColour(bool fill)
{
	if (m_updating)
		return;
	const QList<Node*> nodes = selection();
	if (nodes.isEmpty())
		return;

	// the colour the dialog opens on: whatever the first one selected has, so
	// nudging a colour starts from the colour rather than from black
	Node* first = nodes.first();
	const QColor current = fill
		? (first->fill().style() == Qt::NoBrush ? QColor() : first->fill().color())
		: (first->border().style() == Qt::NoPen ? QColor() : first->border().color());

	// ShowAlphaChannel because a fill that cannot be made see-through is no
	// use for a box drawn round other boxes, which is most of them here.
	QColorDialog dialog(current.isValid() ? current : QColor(Qt::white), this);
	dialog.setOption(QColorDialog::ShowAlphaChannel, fill);
	dialog.setWindowTitle(fill ? QStringLiteral("Fill") : QStringLiteral("Border"));
	if (dialog.exec() != QDialog::Accepted)
		return;

	for (Node* node : nodes)
	{
		if (fill)
			node->setFillRecorded(dialog.currentColor());
		else
			node->setBorderRecorded(dialog.currentColor());
	}
	refresh();
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

