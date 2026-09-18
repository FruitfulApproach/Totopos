#include "core/NodeKind.h"

#include "art/Node.h"
#include "art/Object.h"
#include "art/Category.h"
#include "art/AtomicElement.h"
#include "art/Arrow.h"
#include "art/DiagramScene.h"
#include "core/history/SceneHistory.h"
#include "core/history/Mementos.h"

#include <QGraphicsScene>

namespace
{
	const QString kCategoryPrefix = QStringLiteral("category:");
}

QString NodeKind::object()      { return QStringLiteral("object"); }
QString NodeKind::element()     { return QStringLiteral("element"); }
QString NodeKind::subcategory() { return QStringLiteral("subcategory"); }
QString NodeKind::category()    { return QStringLiteral("category"); }

QString NodeKind::builtIn(const QString& name)
{
	return name.isEmpty() ? category() : kCategoryPrefix + name;
}

QString NodeKind::builtInOf(const QString& id)
{
	return id.startsWith(kCategoryPrefix) ? id.mid(kCategoryPrefix.size()) : QString();
}

bool NodeKind::isCategoryKind(const QString& id)
{
	return id == category() || id == subcategory() || id.startsWith(kCategoryPrefix);
}

QString NodeKind::of(const Node* node)
{
	if (dynamic_cast<const AtomicElement*>(node) != nullptr)
		return element();
	if (auto* cat = dynamic_cast<const Category*>(node))
	{
		if (cat->isSubcategory())
			return subcategory();
		return builtIn(cat->builtInName());
	}
	return object();
}

QString NodeKind::label(const QString& id)
{
	if (id == object())      return QStringLiteral("Generic object");
	if (id == element())     return QStringLiteral("Atomic element");
	if (id == subcategory()) return QStringLiteral("Subcategory");
	if (id == category())    return QStringLiteral("Category");
	const QString name = builtInOf(id);
	return name.isEmpty() ? id : QString("Category: %1").arg(name);
}

QList<NodeKind::Choice> NodeKind::choices(const Node* node)
{
	QList<Choice> list;
	if (node == nullptr || dynamic_cast<const Arrow*>(node) != nullptr)
		return list;   // an arrow is a different question, with its own page

	list << Choice{ object(), label(object()),
		QStringLiteral("An object of the category it is drawn in, and nothing more is claimed about it.") };
	list << Choice{ element(), label(element()),
		QStringLiteral("An element of the node it is drawn in: x in M. It holds nothing, so it is where "
		               "the nesting stops, and it is drawn with a dot beside its name.") };

	// A subcategory of WHAT? Of the category it is drawn in - so it is only
	// on offer when there is one, and the entry says which.
	if (Category* home = node->surroundingCategory())
	{
		list << Choice{ subcategory(), label(subcategory()),
			QString("A subcategory of %1: its objects are %2s of %1 and its arrows are %3s of %1, and so "
			        "is anything drawn inside it. Drawn with a dotted border and a faint fill.")
				.arg(home->id(), home->objectName(), home->morphismName()) };
	}

	list << Choice{ category(), label(category()),
		QStringLiteral("A category of its own, with whatever structure you give it. Double-click inside "
		               "it to place its objects.") };

	for (const QString& name : Category::builtInNames())
	{
		list << Choice{ builtIn(name), label(builtIn(name)),
			QString("The built-in category %1: it knows what its objects and its arrows are, and what "
			        "can be built in it.").arg(name) };
	}
	return list;
}

Node* NodeKind::create(const QString& kindId, const QString& name, Node* like)
{
	if (kindId == element())
		return new AtomicElement(name);
	if (kindId == object())
		return new Object(name);
	if (kindId == subcategory())
	{
		// A subcategory of R-Mod has to BE an R-Mod, or what is placed in it
		// would not be an R-module. So it is built as a copy of the kind of
		// the category it sits in, and then told that it is a part of it.
		Category* home = like != nullptr ? like->surroundingCategory() : nullptr;
		Category* fresh = Category::createLike(home, name);
		fresh->setSubcategory(true);
		return fresh;
	}
	if (const QString built = builtInOf(kindId); !built.isEmpty())
	{
		if (Category* fresh = Category::createBuiltIn(built))
		{
			fresh->setId(name);
			return fresh;
		}
	}
	return new Category(name);
}

void NodeKind::transplant(Node* from, Node* to)
{
	if (from == nullptr || to == nullptr || from == to)
		return;

	QGraphicsItem* parent = from->parentItem();
	QGraphicsScene* scene = from->scene();

	// ---- 1. everything the two shells have in common
	to->setId(from->id());
	if (!from->labelPattern().isEmpty())
		to->setDerivedLabel(from->labelPattern(), from->labelSources());
	if (from->hasChosenStyle())
	{
		to->setFill(from->fill());
		to->setBorder(from->border());
	}
	to->setExistsSuch(from->existsSuch());
	to->setHypothesis(from->isHypothesis());
	to->setCornerRadius(from->cornerRadius());
	to->setCommutesInComponent(from->commutesInComponent());
	to->setRowsExactInComponent(from->rowsExactInComponent());
	to->setColumnsExactInComponent(from->columnsExactInComponent());
	// what a functor wrote on it, so a live mapping still knows its own work
	for (int key = 1; key <= 4; ++key)
	{
		const QVariant value = from->data(key);
		if (value.isValid())
			to->setData(key, value);
	}

	// ---- 2. what only two categories have in common
	Category* fromCat = dynamic_cast<Category*>(from);
	Category* toCat = dynamic_cast<Category*>(to);
	if (fromCat != nullptr && toCat != nullptr)
	{
		// A built-in comes with its own structure, and it is the structure of
		// THAT category: R-Mod is abelian whether or not the thing it replaced
		// was. So the ticked structure only carries over to a category that is
		// not a built-in.
		if (toCat->builtInName().isEmpty() && !fromCat->properties().isEmpty())
			toCat->setProperties(fromCat->properties());
		toCat->setNextObjectIndex(fromCat->nextObjectIndex());
		toCat->setNextArrowIndex(fromCat->nextArrowIndex());
		toCat->setRowsExact(fromCat->rowsExact());
		toCat->setColumnsExact(fromCat->columnsExact());
		// still out of the scene, so these are written straight onto the new
		// shell: nothing is announced twice and nothing is routed away
		toCat->setStatementName(fromCat->statementName());
		toCat->setStatementKind(fromCat->statementKind());
		toCat->setCommutes(fromCat->commutes());
	}

	// ---- 3. where it sits
	if (parent != nullptr)
		to->setParentItem(parent);
	else if (scene != nullptr)
		scene->addItem(to);
	to->setPos(from->pos());
	to->setZValue(from->zValue());
	to->setFlags(from->flags());

	// ---- 4. what it holds. An element holds nothing, so anything drawn
	// inside one stays with the shell being put away, and comes back with it
	// if the change is undone.
	const bool takesChildren = dynamic_cast<AtomicElement*>(to) == nullptr;
	if (takesChildren)
	{
		const QList<QGraphicsItem*> children = from->childItems();
		for (QGraphicsItem* child : children)
		{
			Node* kid = dynamic_cast<Node*>(child);
			if (kid == nullptr)
				continue;   // the shell's own label, which belongs to the shell
			const QPointF where = kid->pos();
			kid->setParentItem(to);
			kid->setPos(where);   // the two shells sit at the same place, so this holds
		}
	}

	// ---- 5. the arrows that end on it, wherever in the diagram they are
	if (scene != nullptr)
	{
		const QList<QGraphicsItem*> items = scene->items();
		for (QGraphicsItem* item : items)
		{
			Arrow* arrow = dynamic_cast<Arrow*>(item);
			if (arrow == nullptr)
				continue;
			if (arrow->domain() == from)
				arrow->setDomain(to);
			if (arrow->codomain() == from)
				arrow->setCodomain(to);
		}
	}

	// ---- 6. the old shell steps out, alive, in case this is undone
	from->setParentItem(nullptr);
	if (from->scene() != nullptr)
		from->scene()->removeItem(from);

	to->refreshDepthAppearance();
	to->refreshFrame();
	if (Node* above = dynamic_cast<Node*>(parent))
		above->refreshFrame();
}

Node* NodeKind::retype(Node* node, const QString& kindId)
{
	if (node == nullptr || kindId.isEmpty() || of(node) == kindId)
		return node;
	if (dynamic_cast<Arrow*>(node) != nullptr)
		return node;

	DiagramScene* diagram = dynamic_cast<DiagramScene*>(node->scene());
	if (diagram == nullptr || diagram->ambientCategory() == node)
		return node;   // the canvas is not one of the things drawn on it

	// nothing may be left pointing at the shell that is about to be put away
	diagram->hideHandles();
	if (diagram->arrowPending())
		diagram->cancelArrow();

	const QString was = label(of(node));
	const bool wasSelected = node->isSelected();

	Node* fresh = create(kindId, node->id(), node);
	transplant(node, fresh);
	fresh->setSelected(wasSelected);

	const QString name = fresh->id().isEmpty() ? QStringLiteral("A node") : fresh->id();
	diagram->history()->record(new NodeRetyped(
		QString("%1 is now %2, was %3").arg(name, label(kindId).toLower(), was.toLower()),
		node, fresh));

	Category* home = fresh->surroundingCategory();
	emit diagram->message(QString("%1 is %2.").arg(name,
		kindId == subcategory() && home != nullptr
			? QString("a subcategory of %1").arg(home->id())
			: label(kindId).toLower()));
	emit diagram->statementChanged(diagram->statementText());
	diagram->checkDiagram();
	return fresh;
}
