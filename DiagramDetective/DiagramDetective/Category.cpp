#include "Category.h"
#include "Arrow.h"
#include "props/MapsElements.h"
#include "DiagramScene.h"
#include "history/SceneHistory.h"
#include "history/Mementos.h"
#include "props/CategoryProps.h"
#include <QMenu>

Category::Category(const QString& name, QGraphicsItem* parent)
	: Object(name, parent)
{
	// a category is a translucent yellow region with a dodger-blue frame
	setFill(QBrush(QColor(255, 235, 0, 55)));
	setBorder(QPen(QColor(30, 144, 255, 170), 2));
}

Category::~Category()
{
	// the props are QObject children: Qt deletes them with us
}

Category* Category::createLike(const Category* model, const QString& name, QGraphicsItem* parent)
{
	// A built-in knows its own class, and that class is what makes its
	// objects and its arrows: a subcategory of R-Mod has to BE an R-Mod, or
	// what is placed in it would not be an R-module.
	if (model != nullptr)
	{
		if (Category* built = createBuiltIn(model->builtInName(), parent))
		{
			built->setId(name);
			return built;
		}
	}
	auto* plain = new Category(name, parent);
	if (model != nullptr)
		plain->setProperties(model->properties());   // a custom category: the structure carries over
	return plain;
}

bool Category::isAmbient() const
{
	auto* diagram = dynamic_cast<DiagramScene*>(scene());
	return diagram != nullptr && diagram->ambientCategory() == this;
}

Category* Category::ambient() const
{
	return m_subcategory ? surroundingCategory() : nullptr;
}

void Category::setSubcategory(bool subcategory)
{
	if (m_subcategory == subcategory)
		return;
	prepareGeometryChange();
	m_subcategory = subcategory;
	// the dotted frame and the fainter fill are set with everything else that
	// follows the nesting, so one call puts both right
	refreshDepthAppearance();
	refreshFrame();
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()))
		emit diagram->statementChanged(diagram->statementText());
}

QString Category::contextTitle() const
{
	if (m_subcategory)
	{
		Category* home = surroundingCategory();
		return home != nullptr
			? QString("Subcategory %1 of %2").arg(id(), home->id())
			: QString("Subcategory %1").arg(id());
	}
	return Object::contextTitle();
}

// ------------------------------------------------ what the diagram in here says

bool Category::commutes() const
{
	// the canvas itself is the scene's business, so the panel over it and this
	// page are never two answers to one question
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()); diagram != nullptr && diagram->ambientCategory() == this)
		return diagram->commutes();
	return m_commutes;
}

void Category::setCommutes(bool commutes)
{
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()); diagram != nullptr && diagram->ambientCategory() == this)
	{
		diagram->setCommutes(commutes);
		return;
	}
	if (m_commutes == commutes)
		return;
	m_commutes = commutes;
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()))
	{
		emit diagram->statementChanged(diagram->statementText());
		emit diagram->message(commutes
			? QString("The diagram in %1 commutes.").arg(id())
			: QString("%1 no longer claims the diagram in it commutes.").arg(id()));
	}
}

int Category::statementKind() const
{
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()); diagram != nullptr && diagram->ambientCategory() == this)
		return int(diagram->statementKind());
	return m_statementKind;
}

void Category::setStatementKind(int kind)
{
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()); diagram != nullptr && diagram->ambientCategory() == this)
	{
		diagram->setStatementKind(DiagramScene::StatementKind(kind));
		return;
	}
	if (m_statementKind == kind)
		return;
	m_statementKind = kind;
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()))
		emit diagram->statementChanged(diagram->statementText());
}

QString Category::statementName() const
{
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()); diagram != nullptr && diagram->ambientCategory() == this)
		return diagram->statementName();
	return m_statementName;
}

void Category::setStatementName(const QString& name)
{
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()); diagram != nullptr && diagram->ambientCategory() == this)
	{
		diagram->setStatementName(name);
		return;
	}
	m_statementName = name;
}

QStringList Category::properties() const
{
	QStringList keys;
	for (const CategoryProp* p : m_props)
		keys << p->key();
	return keys;
}

bool Category::has(const QString& key) const
{
	for (const CategoryProp* p : m_props)
		if (p->key() == key)
			return true;
	return false;
}

void Category::addProperty(const QString& key)
{
	if (has(key))
		return;
	if (CategoryProp* p = CategoryProp::create(key, this))   // owned by us
		m_props.append(p);
	else
		qWarning("Category '%s': unknown property '%s'", qPrintable(id()), qPrintable(key));
}


QString Category::nextSubcategoryName() const
{
	// A subcategory of R-Mod is a CATEGORY, not a module, so it is not named
	// out of the module letters. No counter is kept for it: the first free
	// letter is found by asking what is already spoken for.
	for (int i = 0; i < 256; ++i)
	{
		const QString name = letterName(i, QChar('S'));
		if (!nameInUse(name))
			return name;
	}
	return letterName(0, QChar('S'));
}

Category* Category::createSubcategory(const QPointF& scenePos)
{
	prepareGeometryChange();   // our frame is the union of what we hold
	Category* sub = Category::createLike(this, nextSubcategoryName());
	sub->setSubcategory(true);
	sub->setParentItem(this);
	sub->setPos(mapFromScene(scenePos));
	sub->setZValue(1);
	sub->refreshDepthAppearance();
	sub->refreshFrame();
	refreshFrame();

	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()))
	{
		diagram->recordCreation(QString("Placed the subcategory %1 in %2").arg(sub->id(), id()),
		                        QList<Node*>{ sub });
		emit diagram->message(QString("%1 is a subcategory of %2. Double-click inside it to place its %3s.")
			.arg(sub->id(), id(), objectName()));
	}
	return sub;
}

void Category::populateActions(QMenu& menu)
{
	// A part of this category, placed right here. This is the whole of the
	// nesting: a subcategory is itself a category, so its own menu offers the
	// same entry, and so on down.
	const QPointF where = mapToScene(contextPos());
	QAction* sub = menu.addAction(QString("New subcategory of %1 here").arg(id()));
	sub->setToolTip(QString("A subcategory of %1: its objects are %2s of %1 and its arrows are %3s of %1. "
	                        "Drawn with a dotted border and a faint fill.")
		.arg(id(), objectName(), morphismName()));
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()))
	{
		// queued: the menu is still closing, and this puts a node in the scene
		QObject::connect(sub, &QAction::triggered, diagram, [this, where] {
			QMetaObject::invokeMethod(this, [this, where] { createSubcategory(where); }, Qt::QueuedConnection);
		});
	}
	else
	{
		sub->setEnabled(false);
	}
	menu.addSeparator();

	if (m_props.isEmpty())
		return;

	// Construct, with the groups in a fixed order rather than whatever order
	// the properties happen to be in. Any group nothing asks for is taken out
	// again at the end, so a category with only products shows only Limits.
	auto* construct = new QMenu("Construct", &menu);
	QMenu* limits = construct->addMenu("Limits");
	QMenu* colimits = construct->addMenu("Colimits");
	QMenu* additive = construct->addMenu("Additive");

	const CategoryProp::MenuGroup group = [&](const QString& name) -> QMenu* {
		if (name == "Limits") return limits;
		if (name == "Colimits") return colimits;
		if (name == "Additive") return additive;
		return construct;   // belongs to no group: straight under Construct
	};

	for (CategoryProp* p : m_props)
		p->addConstructions(this, group);

	for (QMenu* sub : { limits, colimits, additive })
	{
		if (sub->isEmpty())
		{
			construct->removeAction(sub->menuAction());
			delete sub;
		}
	}

	if (construct->isEmpty())
	{
		delete construct;
	}
	else
	{
		menu.addMenu(construct);
		menu.addSeparator();
	}

	// what the diagram drawn in here is asserted to be
	menu.addSection(QString("The diagram in %1").arg(id()));
	QAction* rows = menu.addAction("Rows exact");
	rows->setCheckable(true);
	rows->setChecked(m_rowsExact);
	rows->setToolTip("Every row of this diagram is an exact sequence: at each object along it, the image of "
	                 "the arrow coming in is the kernel of the arrow going out.");
	QObject::connect(rows, &QAction::toggled, &menu, [this](bool on) { setRowsExactRecorded(on); });

	QAction* columns = menu.addAction("Columns exact");
	columns->setCheckable(true);
	columns->setChecked(m_columnsExact);
	columns->setToolTip("The same, down each column. Rows and columns are asserted separately.");
	QObject::connect(columns, &QAction::toggled, &menu, [this](bool on) { setColumnsExactRecorded(on); });
	menu.addSeparator();

	// anything a property offers that is not a construction
	for (CategoryProp* p : m_props)
		p->categoryContextMenu(menu, this);
}
void Category::setProperties(const QStringList& keys)
{
	qDeleteAll(m_props);
	m_props.clear();
	for (const QString& key : keys)
		addProperty(key);
}

QString Category::letterName(int index, QChar first)
{
	// index 0 -> first; after Z the alphabet wraps round with one more prime each time
	const bool upper = !first.isLower();
	const int start = first.toUpper().unicode() - 'A';
	const int k = start + index;
	QString name(QChar((upper ? 'A' : 'a') + (k % 26)));
	const int primes = k / 26;
	for (int i = 0; i < primes; ++i)
		name += QChar(0x2032);   // prime
	return name;
}

Object* Category::makeObject(const QString& name)
{
	// An object of a category is a category in its own right: you can
	// double-click into it and place objects there, and into those, without
	// end. A built-in that knows better overrides this.
	return new Category(name, this);   // a child: joins the scene with us
}

Arrow* Category::createArrow(const QString& name, Node* from, Node* to)
{
	auto* arrow = new Arrow(name, from, to, this);
	// An object here is a category in its own right, so what is drawn inside
	// it are its elements - and an arrow carries them over: x in M becomes
	// f(x) in N. Not live by default; a functor is (see MapsElements).
	arrow->addProperty(MapsElements::Key());
	arrow->setZValue(2);
	arrow->refreshDepthAppearance();
	arrow->refreshFrame();
	return arrow;
}

Arrow* Category::createCanvasArrow(Node* from, Node* to)
{
	return createArrow(freshName(m_nextArrowIndex, firstArrowLetter()), from, to);
}

void Category::applyDepthAppearance(int depth)
{
	Node::applyDepthAppearance(depth);
	if (m_subcategory)
	{
		// A subcategory is a PART of the category it is drawn in, not a thing
		// standing in it, and is drawn as such: barely there, and outlined in
		// dots. The colour is the one its ambient category is drawn in, so a
		// subcategory of R-Mod looks like R-Mod seen through it.
		setFill(QBrush(QColor(255, 235, 0, qMax(8, 26 - 5 * depth))));
		QPen dotted(QColor(30, 144, 255, qMax(90, 190 - 18 * depth)), qMax(0.8, 1.8 - 0.2 * depth));
		dotted.setStyle(Qt::DotLine);
		setBorder(dotted);
		return;
	}
	setFill(QBrush(QColor(255, 235, 0, qMax(14, 55 - 9 * depth))));
	setBorder(QPen(QColor(30, 144, 255, qMax(70, 170 - 20 * depth)), qMax(0.6, 2.0 - 0.3 * depth)));
}

void Category::adopt(Node* node, const QPointF& scenePos)
{
	if (node == nullptr)
		return;
	prepareGeometryChange();   // our frame is the union of our children
	node->setParentItem(this);
	node->setPos(mapFromScene(scenePos));   // a child's pos() is in OUR coordinates
	node->refreshDepthAppearance();
	node->refreshFrame();   // it is whole now: our frame can grow to hold it
}

Object* Category::createObject(const QString& name, const QPointF& scenePos)
{
	prepareGeometryChange();
	Object* object = makeObject(name);
	object->setParentItem(this);
	object->setPos(mapFromScene(scenePos));
	object->setZValue(1);
	object->refreshDepthAppearance();
	object->refreshFrame();
	return object;
}

Object* Category::createCanvasObject(const QPointF& scenePos)
{
	// our frame is the union of our children: tell the scene it is about to grow
	prepareGeometryChange();

	// A child's pos() is in the PARENT's coordinates. mapFromScene turns the
	// click, given in scene coordinates, into ours, so the object lands exactly
	// under the cursor wherever this category happens to sit, and stays there
	// when the category later moves (it moves with it).
	Object* object = makeObject(freshName(m_nextIndex, firstLetter()));
	object->setParentItem(this);   // makeObject normally did this already; a custom one might not
	object->setPos(mapFromScene(scenePos));
	object->setZValue(1);   // above the category's fill
	object->refreshDepthAppearance();
	object->refreshFrame();   // it is whole now: our frame can grow to hold it
	return object;
}

namespace
{
	bool nameUsedUnder(const QGraphicsItem* parent, const QString& name)
	{
		for (QGraphicsItem* child : parent->childItems())
		{
			auto* node = dynamic_cast<Node*>(child);
			if (node == nullptr)
				continue;   // a label
			if (node->id() == name || nameUsedUnder(node, name))
				return true;
		}
		return false;
	}
}

bool Category::nameInUse(const QString& name) const
{
	if (name.isEmpty())
		return false;
	const QGraphicsItem* root = this;
	while (root->parentItem() != nullptr)
		root = root->parentItem();
	return root->childItems().isEmpty() ? false : nameUsedUnder(root, name);
}

QString Category::freshName(int& counter, QChar first) const
{
	QString name = letterName(counter++, first);
	// a handful of tries, then take it anyway rather than spin
	for (int guard = 0; guard < 64 && nameInUse(name); ++guard)
		name = letterName(counter++, first);
	return name;
}

// ---------------------------------------------------------------- exactness

void Category::setRowsExact(bool exact)
{
	if (m_rowsExact == exact)
		return;
	m_rowsExact = exact;
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()))
		emit diagram->statementChanged(diagram->statementText());
}

void Category::setColumnsExact(bool exact)
{
	if (m_columnsExact == exact)
		return;
	m_columnsExact = exact;
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()))
		emit diagram->statementChanged(diagram->statementText());
}

void Category::setRowsExactRecorded(bool exact)
{
	if (m_rowsExact == exact)
		return;
	const bool before = m_rowsExact;
	setRowsExact(exact);
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()))
	{
		diagram->history()->record(new ExactnessChanged(
			exact ? QString("%1 has exact rows").arg(id()) : QString("%1 no longer claims exact rows").arg(id()),
			this, true, before, exact));
		emit diagram->message(exact
			? QString("The rows of %1 are exact.").arg(id())
			: QString("%1 no longer claims its rows are exact.").arg(id()));
	}
}

void Category::setColumnsExactRecorded(bool exact)
{
	if (m_columnsExact == exact)
		return;
	const bool before = m_columnsExact;
	setColumnsExact(exact);
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()))
	{
		diagram->history()->record(new ExactnessChanged(
			exact ? QString("%1 has exact columns").arg(id()) : QString("%1 no longer claims exact columns").arg(id()),
			this, false, before, exact));
		emit diagram->message(exact
			? QString("The columns of %1 are exact.").arg(id())
			: QString("%1 no longer claims its columns are exact.").arg(id()));
	}
}
