#include "core/view/ClassicalView.h"

#include <QList>
#include <QHash>
#include <QSet>
#include <algorithm>

#include "art/DiagramScene.h"
#include "art/Category.h"
#include "art/Object.h"
#include "art/AtomicElement.h"
#include "art/Arrow.h"
#include "art/Functor.h"
#include "art/Implies.h"
#include "core/props/MapsElements.h"

namespace
{
	const QString kGiven = QStringLiteral("given");
	const QString kThen = QStringLiteral("then");
	// how far apart the two boxes stand when nobody has said otherwise
	const qreal kBoxGap = 140.0;

	// ---------------------------------------------------------------- reading the rule
	//
	// The same reading Rule::extract makes of a file, made here of the live
	// diagram instead. Solid is the premise; dashed - or inside something
	// dashed - is the conclusion; crossed out in red is premise AND on its way
	// out. Only the outermost mark counts: what is inside a dashed thing is
	// claimed along with it, and what is inside a struck thing goes with it.

	bool claimed(const Node* node, const Node* root)
	{
		if (node->existsSuch())
			return true;
		for (const QGraphicsItem* p = node->parentItem(); p != nullptr && p != root; p = p->parentItem())
			if (const auto* up = dynamic_cast<const Node*>(p); up != nullptr && up->existsSuch())
				return true;
		return false;
	}

	bool struck(const Node* node, const Node* root)
	{
		if (node->markedForDeletion())
			return true;
		for (const QGraphicsItem* p = node->parentItem(); p != nullptr && p != root; p = p->parentItem())
			if (const auto* up = dynamic_cast<const Node*>(p); up != nullptr && up->markedForDeletion())
				return true;
		return false;
	}

	// everything drawn inside `parent`, parents before children
	void collect(const QGraphicsItem* parent, QList<Node*>& out)
	{
		for (QGraphicsItem* child : parent->childItems())
			if (auto* node = dynamic_cast<Node*>(child))
			{
				out << node;
				collect(node, out);
			}
	}

	// ---------------------------------------------------------------- copying

	// A fresh node of the same KIND as `source`, as a child of `parent`. Not a
	// full clone: only what the picture is made of. What a node MEANS - that
	// it is claimed to exist, that it is on its way out - is deliberately left
	// behind, because in this notation those two facts are carried by which
	// box the copy is standing in.
	Node* freshLike(const Node* source, QGraphicsItem* parent)
	{
		const QString id = source->id();
		if (dynamic_cast<const AtomicElement*>(source) != nullptr)
			return new AtomicElement(id, parent);
		if (const auto* category = dynamic_cast<const Category*>(source))
		{
			Category* box = Category::createLike(category, id, parent);
			if (box == nullptr)
				box = new Category(id, parent);
			box->setSubcategory(category->isSubcategory());
			box->setProperties(category->properties());
			return box;
		}
		return new Object(id, parent);
	}

	void copyLook(const Node* source, Node* copy, const QString& side)
	{
		copy->setPos(source->pos());
		copy->setZValue(source->zValue());
		copy->setCornerRadius(source->cornerRadius());
		copy->setLabelOffset(source->labelOffset());
		copy->setCommutesInComponent(source->commutesInComponent());
		copy->setRowsExactInComponent(source->rowsExactInComponent());
		copy->setColumnsExactInComponent(source->columnsExactInComponent());
		// a colour asked for by hand comes over; the default look is the copy's
		// own business, and depends on what it ends up holding
		if (source->hasChosenStyle())
		{
			copy->setFill(source->fill());
			copy->setBorder(source->border());
		}
		// which node this is a copy of, and which half it is standing in: this
		// pair is the whole of how a position is remembered (see the header)
		copy->setData(ClassicalView::SourceKey, source->key());
		copy->setData(ClassicalView::SideKey, side);
	}

	// A copy of `source` and everything drawn inside it, placed in `parent`.
	// `into` comes back as source -> copy for every node made, which is what
	// the arrows are hung on afterwards.
	void copyTree(Node* source, QGraphicsItem* parent, const QString& side,
	              QHash<Node*, Node*>& into)
	{
		// an arrow is not copied here: its ends may not exist yet, and it is
		// hung on afterwards (see copyArrows)
		if (dynamic_cast<Arrow*>(source) != nullptr)
			return;

		Node* copy = freshLike(source, parent);
		copyLook(source, copy, side);
		into.insert(source, copy);
		for (QGraphicsItem* child : source->childItems())
			if (auto* kid = dynamic_cast<Node*>(child); kid != nullptr && dynamic_cast<Arrow*>(kid) == nullptr)
				copyTree(kid, copy, side, into);
	}

	// Every arrow in `arrows` whose two ends have been copied, drawn again
	// between the copies. An arrow whose end did not come over is dropped:
	// there is nothing for it to point at.
	void copyArrows(const QList<Arrow*>& arrows, const QString& side,
	                QHash<Node*, Node*>& into, QGraphicsItem* fallbackParent)
	{
		for (Arrow* source : arrows)
		{
			Node* from = into.value(source->domain(), nullptr);
			Node* to = into.value(source->codomain(), nullptr);
			if (from == nullptr || to == nullptr)
				continue;

			// where the original was drawn, if that came over too; the box
			// itself otherwise
			QGraphicsItem* parent = fallbackParent;
			if (auto* up = dynamic_cast<Node*>(source->parentItem()))
				if (Node* copiedParent = into.value(up, nullptr))
					parent = copiedParent;

			auto* domCat = dynamic_cast<Category*>(from);
			auto* codCat = dynamic_cast<Category*>(to);
			Arrow* copy = nullptr;
			if (dynamic_cast<Functor*>(source) != nullptr && domCat != nullptr && codCat != nullptr)
				copy = new Functor(source->id(), domCat, codCat, parent);
			else
				copy = new Arrow(source->id(), from, to, parent);

			copyLook(source, copy, side);
			copy->setProperties(source->properties());
			copy->setStyle(source->style());
			copy->setBends(source->bends());
			// A COPY NEVER WRITES INTO THE DIAGRAM IT COPIES.
			//
			// A functor kept live would go looking for its domain's elements
			// and draw their images into its codomain - which here means one
			// view of a diagram quietly editing another. The picture is a
			// snapshot; what it shows was already drawn by the real functor.
			if (auto* maps = dynamic_cast<MapsElements*>(copy->prop(MapsElements::Key())))
				maps->setLive(false);
			into.insert(source, copy);
		}
	}

	// the copies in a box, the box included, in no particular order
	void everythingIn(Node* box, QList<Node*>& out)
	{
		out << box;
		collect(box, out);
	}
}

// ---------------------------------------------------------------- the view

ClassicalView::ClassicalView(DiagramScene* scene)
	: m_scene(scene)
{
}

ClassicalView::~ClassicalView()
{
	// The implication first: it hangs off the two boxes and must not be left
	// holding an end that has gone.
	delete m_implies.data();
	delete m_givens.data();
	delete m_conclusion.data();
}

bool ClassicalView::isPartOfView(const Node* node)
{
	for (const QGraphicsItem* p = node; p != nullptr; p = p->parentItem())
		if (p->data(SideKey).isValid())
			return true;
	return dynamic_cast<const Implies*>(node) != nullptr;
}

bool ClassicalView::build(const QHash<QString, QPointF>& remembered)
{
	if (m_scene == nullptr)
		return false;
	Category* ambient = m_scene->ambientCategory();
	if (ambient == nullptr)
		return false;

	// ---- 1. read the diagram as the implication it already is
	QList<Node*> everything;
	collect(ambient, everything);

	QList<Node*> premiseObjects, conclusionObjects;
	QList<Arrow*> premiseArrows, conclusionArrows;
	for (Node* node : everything)
	{
		const bool isConclusion = claimed(node, ambient);
		if (auto* arrow = dynamic_cast<Arrow*>(node))
			(isConclusion ? conclusionArrows : premiseArrows) << arrow;
		else
			(isConclusion ? conclusionObjects : premiseObjects) << node;
	}
	// parents before children, so a parent's copy exists to be copied into
	auto byDepth = [](Node* a, Node* b) {
		auto depth = [](Node* n) { int d = 0; for (QGraphicsItem* p = n->parentItem(); p != nullptr; p = p->parentItem()) ++d; return d; };
		return depth(a) < depth(b);
	};
	std::sort(premiseObjects.begin(), premiseObjects.end(), byDepth);
	std::sort(conclusionObjects.begin(), conclusionObjects.end(), byDepth);

	// ---- 2. the two boxes, hanging off the scene and off nothing else
	m_givens = Category::createLike(ambient, QString());
	m_conclusion = Category::createLike(ambient, QString());
	if (m_givens.isNull() || m_conclusion.isNull())
		return false;
	for (Category* box : { m_givens.data(), m_conclusion.data() })
	{
		box->setId(QString());   // the implication is labelled, its ends are not
		box->setData(SideKey, box == m_givens.data() ? kGiven : kThen);
		box->setFlag(QGraphicsItem::ItemIsSelectable, false);
		m_scene->addItem(box);
	}

	// ---- 3. what goes in them
	//
	// GIVEN holds the premise, crossed-out things and all: what you have to
	// have in front of you for the rule to speak.
	//
	// THEN holds what you are left with: the premise less whatever the rule
	// takes away, plus what it says there then is. That is the picture the
	// user reads as the conclusion - the triangle WITH gf in it, not gf on
	// its own, which would say nothing.
	QHash<Node*, Node*> givenMap;
	for (Node* object : premiseObjects)
		if (object->parentItem() == ambient)
			copyTree(object, m_givens.data(), kGiven, givenMap);
	copyArrows(premiseArrows, kGiven, givenMap, m_givens.data());

	QHash<Node*, Node*> thenMap;
	for (Node* object : premiseObjects)
		if (object->parentItem() == ambient && !struck(object, ambient))
			copyTree(object, m_conclusion.data(), kThen, thenMap);
	for (Node* object : conclusionObjects)
		if (object->parentItem() == ambient)
			copyTree(object, m_conclusion.data(), kThen, thenMap);
	QList<Arrow*> kept;
	for (Arrow* arrow : premiseArrows)
		if (!struck(arrow, ambient))
			kept << arrow;
	kept += conclusionArrows;
	copyArrows(kept, kThen, thenMap, m_conclusion.data());

	// ---- 4. the implication between them
	m_implies = new Implies(m_scene->statementName(), m_givens.data(), m_conclusion.data());
	m_scene->addItem(m_implies.data());

	// ---- 5. where everything stands
	//
	// The boxes take their size from what they hold, so they have to be filled
	// before they can be placed. A position that was set by hand last time
	// wins over the fresh layout - that is the point of remembering them.
	m_givens->refreshFrame();
	m_conclusion->refreshFrame();
	const QRectF left = m_givens->boundingRect();
	m_givens->setPos(0, 0);
	m_conclusion->setPos(left.width() + kBoxGap, 0);

	QList<Node*> placed;
	everythingIn(m_givens.data(), placed);
	everythingIn(m_conclusion.data(), placed);
	for (Node* node : placed)
	{
		const QString side = node->data(SideKey).toString();
		const QString key = node->data(SourceKey).toString();
		// The boxes themselves have a side but no source: they are nobody's
		// copy. They are keyed on the side alone.
		const QString lookup = key.isEmpty() ? side : side + QLatin1Char('/') + key;
		if (side.isEmpty())
			continue;
		if (auto it = remembered.constFind(lookup); it != remembered.constEnd())
			node->setPos(*it);
	}

	m_givens->refreshFrame();
	m_conclusion->refreshFrame();
	m_givens->refreshDepthAppearance();
	m_conclusion->refreshDepthAppearance();
	return true;
}

void ClassicalView::harvestPositions(QHash<QString, QPointF>& into) const
{
	if (m_givens.isNull() || m_conclusion.isNull())
		return;
	QList<Node*> placed;
	everythingIn(m_givens.data(), placed);
	everythingIn(m_conclusion.data(), placed);
	for (Node* node : placed)
	{
		const QString side = node->data(SideKey).toString();
		if (side.isEmpty())
			continue;
		const QString key = node->data(SourceKey).toString();
		into.insert(key.isEmpty() ? side : side + QLatin1Char('/') + key, node->pos());
	}
}

void ClassicalView::refreshName()
{
	if (!m_implies.isNull() && m_scene != nullptr)
		m_implies->setId(m_scene->statementName());
}
