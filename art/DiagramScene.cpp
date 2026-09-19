#include "art/DiagramScene.h"
#include "core/categories/BuiltInCategories.h"
#include "tutor/TutorSession.h"
#include "core/AppSettings.h"
#include "art/NodeHandles.h"
#include "art/AtomicElement.h"
#include "tutor/ElementOpTutor.h"
#include "core/layout/GraphLayoutThread.h"
#include "core/props/Constructions.h"
#include "art/NodeLabel.h"
#include "tutor/ArrowTutor.h"
#include "core/history/SceneHistory.h"
#include "core/history/Mementos.h"
#include "art/Arrow.h"
#include "core/props/MapsElements.h"
#include <QPainter>
#include <QKeyEvent>
#include <QHash>
#include <QSet>
#include <functional>
#include <QGraphicsLineItem>
#include <QApplication>
#include <QGraphicsSceneHoverEvent>
#include <QCursor>
#include <QFontMetricsF>
#include <QDataStream>
#include <QClipboard>
#include <QMimeData>
#include <QDrag>
#include "core/io/SceneFile.h"
#include "core/english/Translation.h"
#include "core/view/ClassicalView.h"
#include <QIODevice>
#include <functional>

namespace
{
	// every node under this one, however deep (defined further down, where the
	// rule overlay that first needed it lives)
	void everyNode(QGraphicsItem* parent, QList<Node*>& out);
}

DiagramScene::DiagramScene(QObject* parent)
	: QGraphicsScene(parent)
{
	// a FIXED scene rect: without one the rect follows the items, and the view
	// re-centres the scene whenever it changes; the first node placed would
	// jump away from the cursor
	setSceneRect(-4000, -4000, 8000, 8000);

	// NO BSP INDEX.
	//
	// Qt keeps a binary space partition tree of the items, to answer "what is
	// in this rectangle" without walking the lot. It is worth having for
	// thousands of items; a diagram has tens, and a linear walk of tens costs
	// nothing measurable.
	//
	// What it does cost is a second copy of every item pointer, kept in step
	// by hand as items move, change shape and are destroyed - and every crash
	// this program has had in the scene has died in that tree, climbing it
	// during a repaint and finding an item that is no longer there. With no
	// index, the scene reads the one list it definitely maintains, and there
	// is no second copy to fall out of step.
	setItemIndexMethod(QGraphicsScene::NoIndex);
	m_history = new SceneHistory(this);
	// undo and redo change what the diagram says, so the sentence follows the
	// history rather than every place that edits the scene
	connect(m_history, &SceneHistory::changed, this, [this] {
		emit statementChanged(statementText());
		checkDiagram();
	});
	// the check itself can be switched off, and the grid setting shares the signal
	connect(&AppSettings::instance(), &AppSettings::changed, this, [this] { checkDiagram(); });
	setAmbientCategory("BigCat");
	connect(&AppSettings::instance(), &AppSettings::changed, this, [this] {
		// sizes changed: the labels are re-sized and every frame re-measured,
		// not merely repainted
		if (m_ambientCategory != nullptr)
		{
			m_ambientCategory->refreshDepthAppearance();
			m_ambientCategory->refreshFrame();
			for (QGraphicsItem* item : items())
				if (auto* arrow = dynamic_cast<Arrow*>(item))
					arrow->refreshFrame();   // its rect follows the head size
		}
		update();
	});
}

void DiagramScene::drawBackground(QPainter* painter, const QRectF& rect)
{
	QGraphicsScene::drawBackground(painter, rect);
	if (!AppSettings::instance().showGrid())
		return;
	const qreal unit = Node::snapUnit();
	if (unit < 4)
		return;
	painter->setPen(QPen(QColor(0, 0, 0, 45), 1.5));
	const qreal x0 = qFloor(rect.left() / unit) * unit;
	const qreal y0 = qFloor(rect.top() / unit) * unit;
	QVector<QPointF> dots;
	for (qreal x = x0; x <= rect.right(); x += unit)
		for (qreal y = y0; y <= rect.bottom(); y += unit)
			dots.append(QPointF(x, y));
	painter->drawPoints(dots.constData(), dots.size());
}

DiagramScene::~DiagramScene()
{
	// out of line, and here rather than in the header, because the unique_ptr
	// holds a type the header only forward-declares
}

// ---------------------------------------------------------------- how it is written

void DiagramScene::toggleNotation()
{
	setNotation(isClassical() ? Notation::Succinct : Notation::Classical);
}

void DiagramScene::setNotation(Notation notation)
{
	if (m_notation == notation)
		return;

	// Nothing half-done survives the switch. A rule laid over the diagram, an
	// arrow being placed, a tutor pointing at something: all of them point at
	// what is about to be hidden.
	endRule();
	hideHandles();
	cancelArrow();
	clearSelection();

	if (m_classical)
	{
		// what was dragged about in the classical view is kept before the view
		// that holds it goes (see classicalPositions)
		m_classical->harvestPositions(m_classicalPositions);
		m_classical.reset();
	}

	m_notation = notation;

	if (m_notation == Notation::Classical)
	{
		auto view = std::make_unique<ClassicalView>(this);
		if (!view->build(m_classicalPositions))
		{
			m_notation = Notation::Succinct;
			emit message(QStringLiteral("There is no diagram to write out the long way."));
			emit notationChanged(false);
			return;
		}
		m_classical = std::move(view);
		if (m_ambientCategory != nullptr)
			m_ambientCategory->setVisible(false);
		emit message(statementName().isEmpty()
			? QStringLiteral("Classical notation: what is given, and what follows from it.")
			: QString("Classical notation: the givens imply \"%1\".").arg(statementName()));
	}
	else if (m_ambientCategory != nullptr)
	{
		m_ambientCategory->setVisible(true);
	}

	emit notationChanged(isClassical());
}

void DiagramScene::syncClassicalPositions()
{
	if (m_classical)
		m_classical->harvestPositions(m_classicalPositions);
}

void DiagramScene::setAmbientCategory(const QString& name)
{
	if (m_ambientCategory != nullptr && m_ambientCategory->id() == name)
		return;

	// SETTLED ONCE ANYTHING IS DRAWN IN IT - SAID HERE, NOT ONLY IN THE COMBOS.
	//
	// Everything on the canvas is an object or an arrow OF this category and
	// would mean something else in another, which is why both dropdowns wear
	// a padlock the moment something is drawn (SketchView::setCategoryLocked,
	// PropertiesDock::refreshCategoryBox). Those are two widgets that have to
	// be told; this is the rule. A disabled combo is a courtesy, not a
	// guarantee - it is refreshed from signals that do not fire for every way
	// a node can appear - and the swap it guards is the most destructive
	// thing in the program: it reparents every node in the diagram and then
	// destroys the canvas they were drawn on.
	if (m_ambientCategory != nullptr && m_ambientCategory->holdsAnything())
	{
		emit message(QString("%1 already holds something, so what it is has settled: what is drawn "
		                     "in it would mean something else in %2. Start a new diagram to draw in %2.")
			.arg(m_ambientCategory->id(), name));
		emit ambientCategoryChanged(m_ambientCategory);   // put the dropdowns back
		return;
	}

	// EVERYTHING POINTING INTO THE OLD CANVAS LETS GO FIRST.
	//
	// What follows reparents every node and then takes the old canvas out of
	// the scene and destroys it. Anything still holding one of those nodes -
	// or drawing a decoration over it - is holding it while the ground moves,
	// and the scene walks the result on its next repaint. That is how this
	// came to abort inside effectiveBoundingRect: a pure virtual call is the
	// scene painting an item that is no longer a whole object.
	if (!m_session.isNull())
		m_session->cancel();
	if (isMoving())
		cancelMove();
	hideHandles();
	hideArrowPreview();

	// The rule overlay keeps RAW pointers into the diagram (RuleMatch holds
	// QHash<Node*, Node*>), and the matches it found are about a diagram in
	// the category that is going away. They are not translated to the new
	// one - they are dropped.
	clearRuleOverlay();
	m_matches.clear();

	// And nothing stays selected across the swap. A selection is a set of
	// pointers the scene holds on its own account, and every one of them is
	// about to be reparented out from under it; the properties panel reads
	// that set on every change, so it must not be left naming the old canvas
	// either.
	clearSelection();

	// a built-in is its own subclass (it knows what it is made of); anything
	// else is a plain category the user defined
	Category* fresh = Category::createBuiltIn(name);
	if (fresh == nullptr)
		fresh = new Category(name);
	fresh->setPos(0, 0);
	// the ambient category IS the canvas: it neither moves nor gets selected,
	// so a press on it starts a rubber band, not a drag of everything
	fresh->setFlag(QGraphicsItem::ItemIsMovable, false);
	fresh->setFlag(QGraphicsItem::ItemIsSelectable, false);
	fresh->setFlag(QGraphicsItem::ItemIsFocusable, false);
	fresh->setZValue(-1);
	addItem(fresh);

	if (m_ambientCategory != nullptr)
	{
		// the objects drawn so far carry over, at the same scene positions
		for (QGraphicsItem* child : m_ambientCategory->childItems())
		{
			auto* node = dynamic_cast<Node*>(child);
			if (node == nullptr)
				continue;   // the category's own label
			fresh->adopt(node, child->scenePos());
		}
		// Out of the scene BEFORE anything else can look at it again: from
		// here it is no longer drawn, hit-tested or indexed, so the turn of
		// the event loop it spends waiting to be destroyed is spent outside
		// the scene entirely.
		removeItem(m_ambientCategory);
		// deleteLater, not delete: this runs from a combo box's signal, and
		// destroying a QObject inside the signal that asked for it is asking
		// for a slot further down the list to find it gone
		m_ambientCategory->deleteLater();
	}

	m_ambientCategory = fresh;
	emit ambientCategoryChanged(fresh);
	emit statementChanged(statementText());
}

Category* DiagramScene::categoryAt(QGraphicsItem* item) const
{
	if (item == nullptr)
		return m_ambientCategory;
	if (auto* category = dynamic_cast<Category*>(item))
		return category;
	// a label is a plain text item parented to its node
	auto* node = dynamic_cast<Node*>(item);
	if (node == nullptr)
	{
		if (auto* owner = dynamic_cast<Node*>(item->parentItem()))
			return dynamic_cast<Category*>(owner) != nullptr
				? static_cast<Category*>(owner)
				: owner->surroundingCategory();
		return nullptr;
	}
	// Some other node - a plain object. It holds nothing itself, so what is
	// placed AT it goes into the category it is drawn in, beside it.
	return node->surroundingCategory();
}

void DiagramScene::beginSession(TutorSession* session)
{
	if (!m_session.isNull() && m_session != session)
		m_session->cancel();
	m_session = session;
}

void DiagramScene::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
	// the right button abandons a move, rather than opening a menu on top of it
	if (isMoving())
	{
		cancelMove();
		event->accept();
		return;
	}

	QGraphicsScene::contextMenuEvent(event);   // an item under the cursor takes it
	if (event->isAccepted())
		return;
	if (m_ambientCategory != nullptr)
	{
		// where on the canvas it was, so "place something here" means here
		m_ambientCategory->popupContextMenu(event->screenPos(),
		                                    m_ambientCategory->mapFromScene(event->scenePos()));
		event->accept();
	}
}

void DiagramScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
	// A DOUBLE-CLICK ON SOMETHING DRAWN STARTS AN ARROW OUT OF IT.
	//
	// It used to be a little button that appeared when the mouse came near a
	// border, which put an icon over the diagram nearly all the time for a
	// gesture wanted once an arrow. The gesture is now the double-click
	// itself, on the thing the arrow is to come out of - its face or its
	// name, both being the same thing to point at.
	//
	// What that displaces is the editor: double-clicking a name no longer
	// opens it. Renaming is on the right-click menu, of the node or of the
	// name (Edit label), where it can say what it does.
	//
	// THE CANVAS IS THE ONE EXCEPTION, AND IT IS THE ONLY ONE.
	//
	// A double-click on the canvas - the ambient category, the yellow behind
	// everything - is where a new object goes, and that is how a diagram is
	// drawn in the first place. Everything ELSE drawn in it is a thing an
	// arrow can come out of, and a double-click on it starts that arrow: on
	// its face or on its name, both being the same thing to point at.
	//
	// That holds however deep the nesting goes and whatever the nodes happen
	// to BE. A category drawn in BigCat is an object of BigCat, so
	// double-clicking it starts a functor; a module drawn in R-Mod is an
	// object of R-Mod, so double-clicking it starts an R-linear map. The face
	// of a nested category used to be treated as room to place into instead,
	// which made the commonest gesture in the program mean two different
	// things depending on how deep you were - and left a stray object inside
	// the very node you were trying to draw an arrow out of.
	//
	// Placing INTO a nested category is still there, on its right-click menu
	// ("New object here", "New subcategory here"), where it can say which
	// category it means.
	//
	// WHILE AN ARROW IS BEING PLACED none of this applies: a double-click
	// then says the other end goes here, and that is handled below.
	Node* pending = m_arrowFrom.data();
	const bool finishing = arrowPending() && pending != nullptr;

	if (!finishing)
	{
		QGraphicsItem* hit = hitItem(event->scenePos());
		auto* label = dynamic_cast<NodeLabel*>(hit);

		// mid-edit the text widget wants its own double-click, to select a word
		if (label != nullptr && label->isEditing())
		{
			QGraphicsScene::mouseDoubleClickEvent(event);
			return;
		}

		Node* from = label != nullptr ? dynamic_cast<Node*>(label->parentItem())
		                              : nodeAt(event->scenePos());
		if (from != nullptr && from != m_ambientCategory && from->canStartArrow())
		{
			beginArrow(from);
			event->accept();
			return;
		}
	}

	// let items under the cursor take the double-click first
	QGraphicsScene::mouseDoubleClickEvent(event);

	// A double-click on the CANVAS, or on the face of a category, puts a new
	// object in: the nearest category AT the cursor is the one it goes into,
	// so double-clicking the canvas places into the canvas and
	// double-clicking inside a category places in that category. Everything
	// drawn has been dealt with above - there, a double-click starts an
	// arrow. An element is put in from the right-click menu (Add element).
	//
	// WHILE AN ARROW IS BEING PLACED a double-click is not a cancel: it says
	// the other end goes HERE. Anywhere here - over the domain's category or
	// well outside it, because the new object goes into the domain's category
	// whatever lies under the cursor. An arrow joins two objects of one
	// category, so there is nowhere else it could go, and that category's
	// frame simply grows to reach the object just put down.
	Node* from = pending;

	Category* category = finishing ? from->surroundingCategory()
							  : categoryAt(hitItem(event->scenePos()));
	if (category == nullptr)
	{
		if (arrowPending())
			cancelArrow();   // nowhere to put the other end
		return;
	}

	// the category decides what it is made of: a category in BigCat, a set in Set, ...
	Object* placed = category->createCanvasObject(event->scenePos());
	if (placed == nullptr)
	{
		if (arrowPending())
			cancelArrow();
		return;
	}
	recordCreation(QString("Placed %1 in %2").arg(placed->id(), category->id()), { placed });
	if (finishing)
		finishArrow(placed);   // and the arrow that was waiting now has an end
	event->accept();
}

// ---------------------------------------------------------------- drawing arrows

QGraphicsItem* DiagramScene::hitItem(const QPointF& scenePos) const
{
	// Skip our own overlays. This is what made a drawn arrow never appear: the
	// dashed preview line runs to the cursor, so the click that was meant to
	// name the codomain landed on the line instead of the object under it.
	const QList<QGraphicsItem*> under = items(scenePos, Qt::IntersectsItemShape, Qt::DescendingOrder);
	for (QGraphicsItem* item : under)
	{
		if (item == m_handle)
			continue;
		// The arrow being placed RUNS TO THE CURSOR, so it is always directly
		// under it - and so is its label. Were it not skipped here, every
		// click meant for the object underneath would land on it instead and
		// the arrow could never be finished.
		if (!m_pending.isNull() && (item == m_pending.data() || m_pending->isAncestorOf(item)))
			continue;
		if (item->acceptedMouseButtons() == Qt::NoButton)
			continue;   // decoration: the tutor's pointer and badges
		return item;
	}
	return nullptr;
}

Node* DiagramScene::nodeAt(const QPointF& scenePos) const
{
	QGraphicsItem* item = hitItem(scenePos);
	while (item != nullptr && dynamic_cast<Node*>(item) == nullptr)
		item = item->parentItem();   // a label hit counts for its node
	return dynamic_cast<Node*>(item);
}

Node* DiagramScene::nodeForPress(const QPointF& scenePos) const
{
	// Topmost first, as the scene will deliver the press. An arrow that does
	// not want it is passed over here as well, so the handle bar and the
	// record of what is being dragged are about the node that will actually
	// move - not about the line lying across it.
	for (QGraphicsItem* item : items(scenePos))
	{
		Node* node = nullptr;
		for (QGraphicsItem* up = item; up != nullptr && node == nullptr; up = up->parentItem())
			node = dynamic_cast<Node*>(up);
		if (node == nullptr)
			continue;
		if (auto* arrow = dynamic_cast<Arrow*>(node);
		    arrow != nullptr && !arrow->takesPressAt(arrow->mapFromScene(scenePos)))
			continue;
		return node;
	}
	return nullptr;
}

void DiagramScene::showHandles(Node* node, const QPointF& itemPos)
{
	if (m_handle == nullptr)
	{
		m_handle = new NodeHandles();
		addItem(m_handle);
		connect(m_handle, &NodeHandles::chaseRequested, this, [this](Node* node) {
			auto* arrow = dynamic_cast<Arrow*>(node);
			auto* maps = arrow != nullptr
				? dynamic_cast<MapsElements*>(arrow->prop(MapsElements::Key())) : nullptr;
			if (maps == nullptr || maps->codomain() == nullptr)
				return;
			// carried across, shown, and KEPT carried: from here the codomain
			// follows the domain
			// one switch does all of it: on show, and kept in step from here
			if (maps->mirrorsGeometry())
				maps->sync();
			else
				maps->setMirrorsGeometry(true);
			emit message(QString("Chasing the elements of %1 into %2, and keeping them there.")
				.arg(arrow->domain() != nullptr ? arrow->domain()->id() : QString(), maps->codomain()->id()));
		});
	}
	m_handle->attach(node, itemPos);
}

void DiagramScene::hideHandles()
{
	if (m_handle != nullptr)
		m_handle->detach();
}

void DiagramScene::recordCreation(const QString& description, const QList<Node*>& nodes)
{
	if (m_history == nullptr || nodes.isEmpty())
		return;

	// While the chase is on, anything drawn is an assumption of the statement
	// being chased: it goes into the hypotheses, and it says so.
	if (m_chasing)
	{
		QStringList named;
		for (Node* node : nodes)
		{
			if (node == nullptr)
				continue;
			node->setHypothesis(true);
			if (!m_hypotheses.contains(node))
				m_hypotheses.append(node);
			if (!node->id().isEmpty())
				named << node->id();
		}
		if (!named.isEmpty())
			emit message(QString("Into the hypotheses: %1").arg(named.join(", ")));
	}

	m_history->record(new NodesCreated(description, nodes));
	emit nodesAdded(nodes);
	emit statementChanged(statementText());
	checkDiagram();
}

void DiagramScene::startChase()
{
	// the setup tutor, if it is still up, has done its job
	if (!m_session.isNull())
		m_session->cancel();
	setChasing(true);
}

void DiagramScene::toggleChase()
{
	if (m_chasing)
		endChase();
	else
		startChase();
}

void DiagramScene::setChasing(bool chasing)
{
	if (m_chasing == chasing)
		return;
	m_chasing = chasing;
	emit chasingChanged(chasing);
	emit message(chasing
		? QStringLiteral("Diagram chase. Anything you add from here is forced into the hypotheses; deleting assumes nothing.")
		: QStringLiteral("Back to let: the diagram is what you are given, and adding to it assumes nothing."));
	emit statementChanged(statementText());
}

void DiagramScene::setCommutes(bool commutes)
{
	if (m_commutes == commutes)
		return;
	m_commutes = commutes;
	emit commutesChanged(commutes);
	emit statementChanged(statementText());
	checkDiagram();   // a ring is only a problem for a diagram that claims to commute
}

namespace
{
	// everything drawn below this node that carries a label
	void collectNamed(QGraphicsItem* parent, QList<Node*>& named)
	{
		for (QGraphicsItem* child : parent->childItems())
		{
			auto* node = dynamic_cast<Node*>(child);
			if (node == nullptr)
				continue;   // a label
			if (!node->id().isEmpty())
				named << node;
			collectNamed(node, named);
		}
	}

	bool isAncestorOf(const QGraphicsItem* ancestor, const QGraphicsItem* item)
	{
		for (const QGraphicsItem* p = item; p != nullptr; p = p->parentItem())
			if (p == ancestor)
				return true;
		return false;
	}

	// Two things drawn one inside the other, or side by side in the same
	// category, are in each other's world: a name there has to mean one of
	// them. Two things in categories neither of which contains the other are
	// in different worlds, and may share a name freely.
	bool sharesAWorld(const Node* a, const Node* b)
	{
		const QGraphicsItem* sa = a->parentItem();
		const QGraphicsItem* sb = b->parentItem();
		if (sa == nullptr || sb == nullptr)
			return false;
		return sa == sb || isAncestorOf(sa, sb) || isAncestorOf(sb, sa);
	}

	// Could these two be ONE THING drawn twice? A zero object at each end of
	// a diagram is written 0 both times, and means the same module both
	// times; that is not a name doing the work of two. It is, when something
	// about them refuses to be identified: one inside the other's world
	// (nothing contains itself), an object against an arrow, or two arrows
	// that do not run between the same-named ends.
	bool couldBeTheSame(const Node* a, const Node* b)
	{
		if (a->parentItem() != b->parentItem())
			return false;
		auto* fa = dynamic_cast<const Arrow*>(a);
		auto* fb = dynamic_cast<const Arrow*>(b);
		if ((fa == nullptr) != (fb == nullptr))
			return false;
		if (fa == nullptr)
			return true;   // two objects side by side: the same one, twice
		// the category's own unnamed arrow - the zero map - may be drawn as
		// often as it is needed, between whatever ends it is needed between
		if (auto* home = dynamic_cast<const Category*>(a->parentItem());
		    home != nullptr && !home->implicitArrowName().isEmpty() && a->id() == home->implicitArrowName())
			return true;
		auto name = [](const Node* end) { return end != nullptr ? end->id() : QString(); };
		return name(fa->domain()) == name(fb->domain())
		    && name(fa->codomain()) == name(fb->codomain());
	}

	QString describe(const Node* node)
	{
		auto* arrow = dynamic_cast<const Arrow*>(node);
		auto* where = dynamic_cast<Category*>(node->parentItem());
		const QString home = where != nullptr ? where->id() : QStringLiteral("the canvas");
		if (arrow == nullptr)
			return QString("the object %1 in %2").arg(node->id(), home);
		return QString("the arrow %1 : %2 %3 %4 in %5")
			.arg(node->id(),
			     arrow->domain() != nullptr ? arrow->domain()->id() : QStringLiteral("?"),
			     QString(QChar(0x2192)),
			     arrow->codomain() != nullptr ? arrow->codomain()->id() : QStringLiteral("?"),
			     home);
	}

	// clear the red from everything below this node
	void clearCycleMarks(QGraphicsItem* parent)
	{
		for (QGraphicsItem* child : parent->childItems())
		{
			if (auto* node = dynamic_cast<Node*>(child))
			{
				node->setError(false);
				clearCycleMarks(node);
			}
		}
	}

	// A directed cycle among the objects of ONE category, following the arrows
	// drawn in it. The ring comes back in `ring`, objects and arrows together,
	// so the whole of it can be shown.
	bool findCycleIn(Category* category, QList<Node*>& ring)
	{
		QList<Node*> objects;
		QHash<Node*, QList<QPair<Node*, Arrow*>>> out;
		for (QGraphicsItem* child : category->childItems())
		{
			if (auto* arrow = dynamic_cast<Arrow*>(child))
			{
				if (arrow->domain() != nullptr && arrow->codomain() != nullptr)
					out[arrow->domain()].append(qMakePair(arrow->codomain(), arrow));
			}
			else if (auto* node = dynamic_cast<Node*>(child))
			{
				objects << node;
			}
		}

		QSet<Node*> onPath, finished;
		QList<QPair<Node*, Arrow*>> stack;   // the node, and the arrow we came in by
		std::function<bool(Node*, Arrow*)> walk = [&](Node* at, Arrow* via) -> bool {
			onPath.insert(at);
			stack.append(qMakePair(at, via));
			for (const QPair<Node*, Arrow*>& edge : out.value(at))
			{
				if (onPath.contains(edge.first))
				{
					// the ring runs from that node round to this one
					int start = 0;
					for (int i = 0; i < stack.size(); ++i)
						if (stack.at(i).first == edge.first) { start = i; break; }
					for (int i = start; i < stack.size(); ++i)
					{
						ring << stack.at(i).first;
						if (i > start && stack.at(i).second != nullptr)
							ring << stack.at(i).second;
					}
					ring << edge.second;   // the arrow that closes it
					return true;
				}
				if (!finished.contains(edge.first) && walk(edge.first, edge.second))
					return true;
			}
			stack.removeLast();
			onPath.remove(at);
			finished.insert(at);
			return false;
		};

		for (Node* node : objects)
			if (!finished.contains(node) && walk(node, nullptr))
				return true;

		// and every category drawn inside this one
		for (Node* node : objects)
			if (auto* inner = dynamic_cast<Category*>(node))
				if (findCycleIn(inner, ring))
					return true;
		return false;
	}
}

namespace
{
	// a composite, written the way it is read: the last arrow travelled first
	QString composite(const QList<Arrow*>& path, bool ring)
	{
		// The zero map ABSORBS: anything composed with 0 is 0, on either side.
		// So a path with the zero map anywhere in it is not written out - it
		// is 0, and that is all it is.
		Category* home = path.isEmpty() ? nullptr : path.first()->surroundingCategory();
		const QString zero = home != nullptr ? home->implicitArrowName() : QString();

		QStringList names;
		for (Arrow* arrow : path)
		{
			// a blank label reads as what the category says it is - 0 in
			// R-Mod - and only an arrow nobody can name shows as ?
			const QString name = arrow->effectiveId();
			if (!zero.isEmpty() && name == zero)
				return zero;
			names.prepend(name.isEmpty() ? QStringLiteral("?") : name);
		}
		return names.join(ring ? QString(" %1 ").arg(QChar(0x2218)) : QString());
	}

	// Every simple path out of `at`, bucketed by where it ends. Simple, so a
	// diagram with a cycle (which cannot commute anyway) still terminates.
	void walkPaths(Node* at, const QHash<Node*, QList<QPair<Node*, Arrow*>>>& out,
	               QList<Arrow*>& sofar, QSet<Node*>& onPath,
	               QHash<Node*, QList<QList<Arrow*>>>& found, int maxLength, int maxPaths,
	               int& budget)
	{
		// Every simple path in a dense diagram is an exponential number of
		// paths. The buckets cap what is KEPT; this caps what is LOOKED AT, so
		// a well-connected diagram costs milliseconds rather than minutes.
		if (sofar.size() >= maxLength || --budget < 0)
			return;
		for (const QPair<Node*, Arrow*>& edge : out.value(at))
		{
			if (onPath.contains(edge.first))
				continue;
			sofar.append(edge.second);
			onPath.insert(edge.first);
			QList<QList<Arrow*>>& bucket = found[edge.first];
			if (bucket.size() < maxPaths)
				bucket.append(sofar);
			walkPaths(edge.first, out, sofar, onPath, found, maxLength, maxPaths, budget);
			onPath.remove(edge.first);
			sofar.removeLast();
		}
	}

	void equationsIn(Category* category, bool ring, QStringList& lines, int& budget)
	{
		QList<Node*> objects;
		QHash<Node*, QList<QPair<Node*, Arrow*>>> out;
		for (QGraphicsItem* child : category->childItems())
		{
			if (auto* arrow = dynamic_cast<Arrow*>(child))
			{
				if (arrow->domain() != nullptr && arrow->codomain() != nullptr)
					out[arrow->domain()].append(qMakePair(arrow->codomain(), arrow));
			}
			else if (auto* node = dynamic_cast<Node*>(child))
			{
				objects << node;
			}
		}

		const QString to = QString(QChar(0x2192));
		for (Node* from : objects)
		{
			if (budget <= 0)
				return;
			QHash<Node*, QList<QList<Arrow*>>> found;
			QList<Arrow*> sofar;
			QSet<Node*> onPath;
			onPath.insert(from);
			int visits = 20000;   // per starting object: plenty for any diagram drawn by hand
			walkPaths(from, out, sofar, onPath, found, 8, 8, visits);

			for (auto it = found.constBegin(); it != found.constEnd(); ++it)
			{
				const QList<QList<Arrow*>>& paths = it.value();
				if (paths.size() < 2)
					continue;   // one way round only: nothing is being asserted
				for (int i = 1; i < paths.size() && budget > 0; ++i)
				{
					const QString left = composite(paths.at(0), ring);
					const QString right = composite(paths.at(i), ring);
					if (left == right)
						continue;   // 0 = 0 says nothing; neither does f = f
					lines << QString("%1 %2 %3:   %4  =  %5")
						.arg(from->id(), to, it.key()->id(), left, right);
					--budget;
				}
			}
		}

		// and the categories drawn inside this one
		for (Node* node : objects)
			if (auto* inner = dynamic_cast<Category*>(node))
				equationsIn(inner, ring, lines, budget);
	}
}

QStringList DiagramScene::commutingEquations(bool composeWithRing) const
{
	QStringList lines;
	if (m_ambientCategory == nullptr)
		return lines;
	int budget = 200;   // a big diagram has a great many paths; enough to read
	equationsIn(m_ambientCategory, composeWithRing, lines, budget);
	lines.removeDuplicates();
	return lines;
}

void DiagramScene::checkDiagram()
{
	if (m_ambientCategory == nullptr)
		return;
	clearCycleMarks(m_ambientCategory);

	const bool look = m_commutes && AppSettings::instance().cycleCheck();
	QList<Node*> ring;
	if (look)
		findCycleIn(m_ambientCategory, ring);

	if (ring.isEmpty())
	{
		// the other thing a diagram can fail at: one name doing the work of two
		const QString clash = AppSettings::instance().nameCheck() ? nameClash() : QString();
		if (clash != m_cycleError)
		{
			m_cycleError = clash;
			emit error(clash);
		}
		return;
	}

	// a ring in a piece that claims nothing is nobody's business
	bool ringCommutes = true;
	for (Node* node : ring)
		if (dynamic_cast<Arrow*>(node) == nullptr && !node->commutesInComponent())
			ringCommutes = false;
	if (!ringCommutes)
	{
		if (!m_cycleError.isEmpty())
		{
			m_cycleError.clear();
			emit error(QString());
		}
		return;
	}

	QStringList names;
	for (Node* node : ring)
	{
		node->setError(true);
		if (dynamic_cast<Arrow*>(node) == nullptr && !node->id().isEmpty())
			names << node->id();
	}
	if (!names.isEmpty())
		names << names.first();   // show it closing back on itself

	m_cycleError = QString(
		"Cycle %1. A diagram that commutes is read here by comparing the paths between each pair of "
		"objects; going round a ring gives endlessly many paths, so this is not a diagram we can read "
		"as commuting. Break the ring, or turn Commutes off - a diagram that may or may not commute "
		"is free to go round in circles.")
		.arg(names.join(QString(" %1 ").arg(QChar(0x2192))));
	emit error(m_cycleError);
}

void DiagramScene::noteArrowStyle(Arrow* arrow, Arrow::Style before, Arrow::Style after)
{
	if (arrow == nullptr || m_history == nullptr)
		return;
	const QString id = arrow->id().isEmpty() ? QStringLiteral("an arrow") : arrow->id();
	// the menu entry carries the sign after the word; the history wants only
	// the word, so it reads as a sentence
	const QString what = Arrow::styleName(after).section(QLatin1Char(' '), 0, 0).toLower();
	m_history->record(new ArrowStyleChanged(
		after == Arrow::Style::Plain
			? QString("%1 is a plain arrow again").arg(id)
			: QString("%1 is %2 %3").arg(id, what.startsWith(QLatin1Char('i')) ? "an" : "a", what),
		arrow, int(before), int(after)));
	emit message(after == Arrow::Style::Plain
		? QString("%1 is drawn plain again.").arg(id)
		: QString("%1: %2").arg(id, Arrow::styleDescription(after)));
}

void DiagramScene::noteExistsSuch(Node* node, bool before, bool after)
{
	if (node == nullptr || m_history == nullptr)
		return;
	const QString id = node->id().isEmpty() ? QStringLiteral("a node") : node->id();
	m_history->record(new ExistsSuchChanged(
		after ? QString("%1 is claimed to exist").arg(id) : QString("%1 is given again").arg(id),
		node, before, after));
	emit message(after
		? QString("%1 is now the existential part: dotted.").arg(id)
		: QString("%1 is given again.").arg(id));
	// the history's own signal refreshes the statement
}

void DiagramScene::noteDeleteMark(Node* node, bool before, bool after)
{
	if (node == nullptr || m_history == nullptr)
		return;
	const QString id = node->id().isEmpty() ? QStringLiteral("a node") : node->id();
	m_history->record(new DeleteMarkChanged(
		after ? QString("%1 is struck off").arg(id) : QString("%1 is kept again").arg(id),
		node, before, after));
	emit message(after
		? QString("%1 is crossed out: where this diagram is applied as a rule, %1 is taken out.").arg(id)
		: QString("%1 is kept again.").arg(id));
}

namespace
{
	// One thing the diagram quantifies over: what it is called, whether it is
	// an arrow, and WHICH CATEGORY it is drawn in - because "a 0 inside N" and
	// "a 0 beside N" are different statements, and the nesting is what says
	// which one this is.
	struct Quantified
	{
		QString name;
		bool isArrow = false;
		QString home;   // empty when it is drawn in the ambient category itself
	};

	// everything drawn inside a node, however deep, split into what is given
	// and what is claimed to exist
	void collectStatement(const QGraphicsItem* parent, const Category* ambient,
	                      QList<Quantified>& given, QList<Quantified>& claimed)
	{
		for (QGraphicsItem* child : parent->childItems())
		{
			auto* node = dynamic_cast<Node*>(child);
			if (node == nullptr)
				continue;   // a label
			if (!node->id().isEmpty())
			{
				auto* home = dynamic_cast<Category*>(node->parentItem());
				Quantified item;
				item.name = node->id();
				item.isArrow = dynamic_cast<Arrow*>(node) != nullptr;
				if (home != nullptr && home != ambient)
					item.home = home->id();
				(node->existsSuch() ? claimed : given) << item;
			}
			collectStatement(node, ambient, given, claimed);
		}
	}

	// "N, M : objects" and "0 : object in N", like things read together
	QString phrase(const QList<Quantified>& items, const QString& sign)
	{
		QStringList parts;
		int at = 0;
		while (at < items.size())
		{
			const bool isArrow = items.at(at).isArrow;
			const QString home = items.at(at).home;
			QStringList names;
			while (at < items.size() && items.at(at).isArrow == isArrow && items.at(at).home == home)
				names << items.at(at++).name;

			QString part = QString("%1 %2 : %3").arg(sign, names.join(", "),
				names.size() == 1 ? (isArrow ? "arrow" : "object") : (isArrow ? "arrows" : "objects"));
			if (!home.isEmpty())
				part += " in " + home;
			parts << part;
		}
		return parts.join(", ");
	}
}

QString DiagramScene::statementText() const
{
	if (m_ambientCategory == nullptr)
		return QString();

	QList<Quantified> givenItems, claimedItems;
	collectStatement(m_ambientCategory, m_ambientCategory, givenItems, claimedItems);
	if (givenItems.isEmpty() && claimedItems.isEmpty())
		return QString("Draw the setup in %1: double-click the canvas to place an object.").arg(m_ambientCategory->id());

	const QString forAll = QString(QChar(0x2200));
	const QString exists = QString(QChar(0x2203));
	const QString implies = QString(QChar(0x21D2));

	QString given = phrase(givenItems, forAll);
	const QString claimed = phrase(claimedItems, exists);

	// which pieces of the diagram claim exactness, and of what - and which
	// nodes claim it of the diagram drawn inside them
	QStringList exactRows, exactColumns;
	for (const Component& piece : components())
	{
		if (piece.rowsExact) exactRows << piece.title;
		if (piece.columnsExact) exactColumns << piece.title;
	}
	QStringList commuting, subcategories;
	{
		QList<Node*> everything;
		collectNamed(m_ambientCategory, everything);
		const QString subsetOf = QString(QChar(0x2286));
		for (Node* node : everything)
		{
			auto* home = dynamic_cast<Category*>(node);
			if (home == nullptr)
				continue;
			if (home->isSubcategory())
			{
				Category* over = home->ambient();
				subcategories << (over != nullptr
					? QString("%1 %2 %3").arg(home->id(), subsetOf, over->id())
					: home->id());
			}
			if (!home->holdsAnything())
				continue;
			if (home->rowsExact()) exactRows << QString("the diagram in %1").arg(home->id());
			if (home->columnsExact()) exactColumns << QString("the diagram in %1").arg(home->id());
			if (home->commutes()) commuting << QString("the diagram in %1").arg(home->id());
		}
	}
	if (!subcategories.isEmpty())
		given += QString("%1 with %2").arg(given.isEmpty() ? "" : ",", subcategories.join(", "));
	if (!commuting.isEmpty())
		given += QString("%1 with %2 commuting").arg(given.isEmpty() ? "" : ",", commuting.join(", "));
	if (!exactRows.isEmpty())
		given += QString("%1 with exact rows in %2").arg(given.isEmpty() ? "" : ",", exactRows.join(", "));
	if (!exactColumns.isEmpty())
		given += QString("%1 with exact columns in %2").arg(given.isEmpty() ? "" : ",", exactColumns.join(", "));

	// The rule the picture states. With a commuting claim it is an
	// IMPLICATION: what is drawn solid commutes, and then there is the dotted
	// part making a diagram that commutes still. Without one there is nothing
	// to imply - it simply says what there is.
	QString sentence = given;
	if (claimed.isEmpty())
	{
		if (m_commutes && !sentence.isEmpty())
			sentence += " : the diagram commutes";
	}
	else if (m_commutes)
	{
		if (!sentence.isEmpty())
			sentence += " such that the diagram commutes  " + implies + "  ";
		sentence += claimed + " such that it commutes still";
	}
	else
	{
		if (!sentence.isEmpty())
			sentence += ", ";
		sentence += claimed;
	}

	if (m_chasing)
		sentence += QString("   [chasing: %1 hypothes%2 added]")
			.arg(m_hypotheses.size()).arg(m_hypotheses.size() == 1 ? "is" : "es");
	sentence += ".";

	// "Axiom - Additive identity exists.  For all N ..."
	if (m_kind != Unstated)
	{
		const QString heading = m_statementName.isEmpty()
			? kindName(m_kind)
			: QString("%1 %2 %3").arg(kindName(m_kind), QString(QChar(0x2014)), m_statementName);
		sentence = heading + ".   " + sentence;
	}
	return sentence;
}

void DiagramScene::deleteNode(Node* node)
{
	deleteNodes({ node });
}

void DiagramScene::deleteNodes(const QList<Node*>& nodes)
{
	QList<Node*> doomed;
	for (Node* node : nodes)
		if (node != nullptr && node != m_ambientCategory && !doomed.contains(node))
			doomed << node;
	// Nothing in the classical view can be deleted: what is drawn there is a
	// COPY of the diagram, and taking a copy away would say nothing about the
	// diagram while looking exactly as though it had. Switch back and delete
	// the thing itself.
	if (m_classical)
	{
		const int before = doomed.size();
		QList<Node*> real;
		for (Node* node : doomed)
			if (!ClassicalView::isPartOfView(node))
				real << node;
		doomed = real;
		if (doomed.isEmpty() && before > 0)
		{
			emit message(QStringLiteral("That is a copy, drawn to show what the diagram says. "
			                            "Switch back to the succinct notation to change the diagram itself."));
			return;
		}
	}
	if (doomed.isEmpty())
		return;

	if (arrowPending())
		cancelArrow();
	hideHandles();

	QString what;
	if (doomed.size() == 1)
	{
		Node* node = doomed.first();
		const QString id = node->id().isEmpty() ? QStringLiteral("a node") : node->id();
		Category* home = node->surroundingCategory();
		what = home != nullptr ? QString("%1 from %2").arg(id, home->id()) : id;
	}
	else
	{
		what = QString("%1 nodes").arg(doomed.size());
	}

	// the memento takes them out and HOLDS them: undo puts them back, and the
	// arrows that end on them travel with them
	auto* removal = new NodesRemoved(QString("Deleted %1").arg(what), doomed);
	removal->redo();
	m_history->record(removal);   // its signal refreshes the statement
	emit nodesRemoved(doomed);
	checkDiagram();
	emit message(QString("Deleted %1. Edit > Undo puts it back.").arg(what));
}

void DiagramScene::clearDiagram()
{
	// The classical view is a picture OF this diagram, so it goes with it -
	// and it goes first, before the nodes it is a picture of are destroyed.
	// The positions are NOT kept: they belong to the diagram being cleared
	// away, and the one about to be read in has its own.
	m_classical.reset();
	m_classicalPositions.clear();
	if (m_notation != Notation::Succinct)
	{
		m_notation = Notation::Succinct;
		emit notationChanged(false);
	}
	if (m_ambientCategory != nullptr)
		m_ambientCategory->setVisible(true);

	endRule();
	hideHandles();
	cancelArrow();
	// a tutor pointing into the diagram has nothing to point at once it is gone
	if (!m_session.isNull())
		m_session->cancel();
	if (m_ambientCategory != nullptr)
	{
		// arrows first: nothing must be left pointing at an object that has gone
		const QList<QGraphicsItem*> children = m_ambientCategory->childItems();
		for (QGraphicsItem* child : children)
			if (dynamic_cast<Arrow*>(child) != nullptr)
				delete child;
		const QList<QGraphicsItem*> rest = m_ambientCategory->childItems();
		for (QGraphicsItem* child : rest)
			if (dynamic_cast<Node*>(child) != nullptr)
				delete child;
	}
	if (m_history != nullptr)
		m_history->clear();
}

void DiagramScene::setArrowPreviewSource(Node* from)
{
	m_arrowFrom = from;
	if (from == nullptr)
	{
		hideArrowPreview();
		return;
	}
	if (m_pending.isNull())
	{
		// The real arrow, with its codomain not yet set, named what it will be
		// named. NOT a child of the category: a child counts towards the
		// category's frame, and the frame would breathe in and out with every
		// mouse move - carrying the category's label about with it.
		Category* home = from->surroundingCategory();
		auto* pending = new Arrow(home != nullptr ? home->nextArrowName() : QString(),
		                          from, nullptr, nullptr);
		pending->setZValue(9999);
		// It lies under the cursor the whole time it exists, so it must never
		// take a click nor be found by hitItem - and neither must its label,
		// which rides the same line.
		pending->setInert();
		addItem(pending);
		m_pending = pending;
	}
	m_pending->setLooseEnd(from->sceneBoundingRect().center());
	m_pending->show();
}

void DiagramScene::hideArrowPreview()
{
	m_arrowFrom = nullptr;
	if (!m_pending.isNull())
	{
		removeItem(m_pending.data());
		delete m_pending.data();
	}
	m_pending = nullptr;
}

void DiagramScene::beginElementOp(Node* from, ElementOp operation)
{
	if (from == nullptr)
		return;
	const ElementOpTutor::Operation which =
		  operation == ElementOp::Plus   ? ElementOpTutor::Operation::Plus
		: operation == ElementOp::Equals ? ElementOpTutor::Operation::Equals
		                                 : ElementOpTutor::Operation::Minus;
	auto* tutor = new ElementOpTutor(this, from, which);
	if (TutorSession* session = tutor->teach(this))
		connect(session, &TutorSession::ended, tutor, &QObject::deleteLater);
	else
		tutor->deleteLater();
}

void DiagramScene::beginArrow(Node* from)
{
	if (from == nullptr)
		return;
	// The arrow tutor takes the clicks from here. With tutor mode off it runs
	// just the same, only without the remarks and the pointer.
	auto* tutor = new ArrowTutor(this, from);
	if (TutorSession* session = tutor->teach(this))
		connect(session, &TutorSession::ended, tutor, &QObject::deleteLater);
	else
		tutor->deleteLater();
}

void DiagramScene::cancelArrow()
{
	if (!m_session.isNull())
		m_session->cancel();   // -> ArrowTutor::onCancel -> hideArrowPreview
	hideArrowPreview();
}

Arrow* DiagramScene::finishArrow(Node* to)
{
	Node* from = m_arrowFrom.data();
	hideArrowPreview();
	if (from == nullptr || to == nullptr)
		return nullptr;

	// the arrow belongs to the category the two objects are drawn in
	Category* home = from->surroundingCategory();
	if (home == nullptr || home != to->surroundingCategory())
	{
		emit message(QString("%1 and %2 are not in the same category: an arrow joins two objects of one category.")
			.arg(from->id(), to->id()));
		return nullptr;
	}
	Arrow* arrow = home->createCanvasArrow(from, to);
	recordCreation(QString("%1 : %2 %3 %4 in %5")
		.arg(arrow->id(), from->id(), QString(QChar(0x2192)), to->id(), home->id()), { arrow });
	hideHandles();
	emit message(QString("%1 : %2 %3 %4 in %5")
		.arg(arrow->id(), from->id(), QString(QChar(0x2192)), to->id(), home->id()));
	return arrow;
}

void DiagramScene::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
	m_lastScenePos = event->scenePos();

	// The border button is up, so the cursor is on it: this press is the one
	// gesture it offers. Taken here, before any of the press machinery below,
	// so nothing starts a move or a selection underneath it.
	// While an arrow is being drawn the tutor session filters the presses, so
	// this only ever runs for ordinary clicks.
	if (event->button() == Qt::LeftButton)
	{
		// NodeHandles ignores transformations, so itemAt with an identity
		// device transform does not test it where it is actually drawn. Ask
		// the item itself instead.
		const bool onHandle = m_handle != nullptr && m_handle->isVisible() && m_handle->isUnderMouse();
		if (!onHandle)   // pressing the handle itself must reach the handle
		{
			Node* node = nodeForPress(event->scenePos());
			// an arrow gets the same bar: delete it, or draw an arrow TO it
			if (node != nullptr && node != m_ambientCategory)
				showHandles(node, node->mapFromScene(event->scenePos()));
			else
				hideHandles();
		}

		// from here until the button comes up, a node that moves is a node the
		// user is moving - and only those shove their neighbours
		Node::setUserDragging(true);
		m_pressed = nullptr;
		m_gesture = Gesture::None;
		m_dragCarriedOff = false;

		// remember where everything that could be dragged is standing, so the
		// move can be recorded as one change when the button comes back up
		m_dragFrom.clear();
		QList<Node*> watched;
		if (Node* node = nodeForPress(event->scenePos()))
			watched << node;
		for (QGraphicsItem* item : selectedItems())
			if (auto* node = dynamic_cast<Node*>(item))
				if (!watched.contains(node))
					watched << node;
		for (Node* node : watched)
			m_dragFrom.append(qMakePair(QPointer<Node>(node), node->pos()));
	}
	QGraphicsScene::mousePressEvent(event);
}

void DiagramScene::notePushed(Node* node, const QPointF& before)
{
	// no drag, no record: this list is only read when the button comes up, so
	// anything added outside a gesture would sit there for ever
	if (node == nullptr || !Node::userDragging())
		return;
	for (const auto& start : m_dragFrom)
		if (start.first == node)
			return;   // already noted where this one started
	m_dragFrom.append(qMakePair(QPointer<Node>(node), before));
}

void DiagramScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
	m_lastScenePos = event->scenePos();
	// the press turned into carrying a copy away: the drag has already had it
	if (m_dragCarriedOff)
	{
		m_dragCarriedOff = false;
		Node::setUserDragging(false);
		event->accept();
		return;
	}

	// a move ends where the button comes up, as one change in the history
	if (isMoving())
	{
		Node* node = m_pressed.data();
		m_pressed = nullptr;
		m_gesture = Gesture::None;
		QApplication::restoreOverrideCursor();
		// A DRAG IN THE CLASSICAL VIEW IS NOT A STEP IN THIS DIAGRAM'S HISTORY.
		//
		// What is dragged there is a copy, and the copy is thrown away and
		// built afresh every time the notation is switched. An entry pointing
		// at one would be an entry that can never be undone, because by the
		// time anybody asked, the thing it names is gone. Where the copy was
		// put is kept instead - see classicalPositions - which is the right
		// place for it: it is a layout, not a change to the diagram.
		const bool inView = m_classical && ClassicalView::isPartOfView(node);
		if (m_moved && node->pos() != m_moveFrom && m_history != nullptr && !inView)
			m_history->record(new ItemsMoved(QString("Moved %1").arg(node->id()),
				{ ItemsMoved::Move{ QPointer<Node>(node), m_moveFrom, node->pos() } }));
		Node::setUserDragging(false);
		event->accept();
		return;
	}
	m_pressed = nullptr;
	m_gesture = Gesture::None;

	QGraphicsScene::mouseReleaseEvent(event);
	Node::setUserDragging(false);

	QList<ItemsMoved::Move> moves;
	for (const auto& start : m_dragFrom)
	{
		if (start.first.isNull() || start.first->pos() == start.second)
			continue;
		if (m_classical && ClassicalView::isPartOfView(start.first.data()))
			continue;   // a copy, not the diagram: see the note above
		moves.append(ItemsMoved::Move{ start.first, start.second, start.first->pos() });
	}
	m_dragFrom.clear();
	if (moves.isEmpty() || m_history == nullptr)
		return;
	const QString what = moves.size() == 1
		? QString("Moved %1").arg(moves.first().node->id())
		: QString("Moved %1 items").arg(moves.size());
	m_history->record(new ItemsMoved(what, moves));
}

void DiagramScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
	m_lastScenePos = event->scenePos();   // where a paste with no point of its own lands

	if (arrowPending() && !m_pending.isNull())
		m_pending->setLooseEnd(event->scenePos());

	if (!m_pressed.isNull() && m_gesture != Gesture::None)
	{
		const bool gone = QLineF(m_pressScenePos, event->scenePos()).length() > 6.0;

		// Ctrl and drag CARRIES A COPY: out of this scene altogether, if that
		// is where it is let go of - another tab, or another copy of the
		// program. The drag itself has to start from a widget, so the view is
		// asked to do the carrying.
		if (gone && !m_dragCarriedOff && (event->modifiers() & Qt::ControlModifier))
		{
			QList<Node*> carried = selectedNodes();
			if (carried.isEmpty() && !m_pressed.isNull())
				carried << m_pressed.data();
			const QByteArray payload = SceneFile::copyFragment(carried);
			m_pressed = nullptr;
			m_gesture = Gesture::None;
			m_moved = false;
			Node::setUserDragging(false);
			if (!payload.isEmpty())
			{
				m_dragCarriedOff = true;
				emit fragmentDragRequested(payload);
			}
			event->accept();
			return;
		}

		// the node goes where the mouse goes, on the grid
		if (m_gesture == Gesture::Move)
		{
			if (gone || m_moved)
			{
				m_moved = true;
				m_pressed->setPos(inParentOf(m_pressed.data(), event->scenePos()) + m_moveGrab);
			}
			event->accept();
			return;
		}
	}

	QGraphicsScene::mouseMoveEvent(event);
}

bool DiagramScene::isEditingLabel() const
{
	auto* text = dynamic_cast<QGraphicsTextItem*>(focusItem());
	return text != nullptr && text->textInteractionFlags() != Qt::NoTextInteraction;
}

void DiagramScene::keyPressEvent(QKeyEvent* event)
{
	// while a label is being typed in, the keys are the label's: Backspace
	// deletes a character, not the node, and Escape reverts the edit
	if (isEditingLabel())
	{
		QGraphicsScene::keyPressEvent(event);
		return;
	}

	if (event->key() == Qt::Key_Escape && isMoving())
	{
		cancelMove();
		event->accept();
		return;
	}
	if (event->key() == Qt::Key_Escape && ruleActive())
	{
		endRule();
		emit message("Rule taken off.");
		event->accept();
		return;
	}

	// the clipboard. Guarded by the label check above: while a label is being
	// typed in, Ctrl+C is the label's.
	if (event->matches(QKeySequence::Copy))
	{
		copySelection();
		event->accept();
		return;
	}
	if (event->matches(QKeySequence::Cut))
	{
		cutSelection();
		event->accept();
		return;
	}
	if (event->matches(QKeySequence::Paste))
	{
		paste();
		event->accept();
		return;
	}
	if (event->key() == Qt::Key_D && (event->modifiers() & Qt::ControlModifier))
	{
		duplicateSelection();
		event->accept();
		return;
	}
	if (event->matches(QKeySequence::SelectAll))
	{
		selectEverything();
		event->accept();
		return;
	}

	if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
	{
		QList<Node*> selected;
		for (QGraphicsItem* item : selectedItems())
			if (auto* node = dynamic_cast<Node*>(item))
				selected << node;
		if (!selected.isEmpty())
		{
			deleteNodes(selected);
			event->accept();
			return;
		}
	}
	if (event->key() == Qt::Key_Escape && arrowPending())
	{
		cancelArrow();
		emit message("Arrow cancelled.");
		event->accept();
		return;
	}

	// The arrow keys carry the selection about. This is how a node that holds
	// things is moved: its label is its own to drag, so it is not the handle
	// the node is carried by. A step is one square of the grid, or one point
	// with Shift held, for putting something exactly where it is wanted.
	{
		QPointF step;
		switch (event->key())
		{
		case Qt::Key_Left:  step = QPointF(-1, 0); break;
		case Qt::Key_Right: step = QPointF(1, 0); break;
		case Qt::Key_Up:    step = QPointF(0, -1); break;
		case Qt::Key_Down:  step = QPointF(0, 1); break;
		default: break;
		}
		if (!step.isNull())
		{
			const qreal unit = (event->modifiers() & Qt::ShiftModifier) || !Node::snapEnabled()
				? 1.0 : qMax(1.0, Node::snapUnit());
			if (nudgeSelection(step * unit))
			{
				event->accept();
				return;
			}
		}
	}

	QGraphicsScene::keyPressEvent(event);
}

bool DiagramScene::nudgeSelection(const QPointF& delta)
{
	QList<Node*> moving;
	for (QGraphicsItem* item : selectedItems())
		if (auto* node = dynamic_cast<Node*>(item))
			if (node != m_ambientCategory && dynamic_cast<Arrow*>(node) == nullptr)
				moving << node;
	if (moving.isEmpty())
		return false;

	// one entry in the history for the lot, so a run of taps on the key can be
	// walked back one tap at a time
	QList<ItemsMoved::Move> moves;
	for (Node* node : moving)
	{
		ItemsMoved::Move move;
		move.node = node;
		move.before = node->pos();
		move.after = node->pos() + delta;
		node->setPos(move.after);
		node->refreshFrame();
		moves.append(move);
	}
	m_history->record(new ItemsMoved(
		moving.size() == 1
			? QString("Moved %1").arg(moving.first()->id().isEmpty() ? QStringLiteral("a node") : moving.first()->id())
			: QString("Moved %1 nodes").arg(moving.size()),
		moves));
	return true;
}

QString DiagramScene::nameClash()
{
	if (m_ambientCategory == nullptr)
		return QString();

	QList<Node*> named;
	collectNamed(m_ambientCategory, named);

	QHash<QString, QList<Node*>> byName;
	for (Node* node : named)
		byName[node->id()].append(node);

	for (auto it = byName.constBegin(); it != byName.constEnd(); ++it)
	{
		const QList<Node*>& sharing = it.value();
		if (sharing.size() < 2)
			continue;
		for (int i = 0; i < sharing.size(); ++i)
		{
			for (int j = i + 1; j < sharing.size(); ++j)
			{
				Node* a = sharing.at(i);
				Node* b = sharing.at(j);
				if (!sharesAWorld(a, b))
					continue;   // different worlds: one name may mean two things
				if (couldBeTheSame(a, b))
					continue;   // one thing drawn twice, as 0 so often is

				a->setError(true);
				b->setError(true);
				const QString why = a->parentItem() == b->parentItem()
					? QStringLiteral("They sit side by side but do not run between the same things, so they are "
					                 "two arrows, not one drawn twice.")
					: QStringLiteral("One is drawn inside the other's world, and nothing contains itself - the "
					                 "chain of containment has no rings - so these cannot be the same thing.");
				return QString("%1 names two things: %2, and %3. %4 Rename one of them.")
					.arg(it.key(), describe(a), describe(b), why);
			}
		}
	}
	return QString();
}

QList<Node*> DiagramScene::labelledNodes(const Node* except) const
{
	QList<Node*> named;
	if (m_ambientCategory == nullptr)
		return named;
	collectNamed(m_ambientCategory, named);
	named.removeAll(const_cast<Node*>(except));
	return named;
}

// ---------------------------------------------------------------- what it is put forward as

QString DiagramScene::kindName(StatementKind kind)
{
	switch (kind)
	{
	case Axiom:      return QStringLiteral("Axiom");
	case Definition: return QStringLiteral("Definition");
	case Theorem:    return QStringLiteral("Theorem");
	case Conjecture: return QStringLiteral("Conjecture");
	case Remark:     return QStringLiteral("Remark");
	case Proof:      return QStringLiteral("Proof");
	default:         return QStringLiteral("Unstated");
	}
}

QStringList DiagramScene::kindNames()
{
	return { kindName(Unstated), kindName(Axiom), kindName(Definition),
	         kindName(Theorem), kindName(Conjecture), kindName(Remark), kindName(Proof) };
}

void DiagramScene::setStatementKind(StatementKind kind)
{
	if (m_kind == kind)
		return;
	const StatementKind before = m_kind;
	m_kind = kind;
	if (m_history != nullptr)
		m_history->record(new StatementDeclared(
			kind == Unstated
				? QStringLiteral("No longer put forward as anything")
				: QString("Put forward as %1%2").arg(kindName(kind).toLower(),
					m_statementName.isEmpty() ? QString() : QString(": %1").arg(m_statementName)),
			this, before, kind, m_statementName, m_statementName));
	emit statementKindChanged(int(m_kind), m_statementName);
	emit statementChanged(statementText());
}

void DiagramScene::setStatementName(const QString& name)
{
	if (m_statementName == name)
		return;
	const QString before = m_statementName;
	m_statementName = name;
	if (m_history != nullptr)
		m_history->record(new StatementDeclared(
			QString("Called it \"%1\"").arg(name), this, m_kind, m_kind, before, name));
	emit statementKindChanged(int(m_kind), m_statementName);
	emit statementChanged(statementText());
	// the name rides the implication arrow: rename the rule and the arrow says so
	if (m_classical)
		m_classical->refreshName();
}

void DiagramScene::setProves(const QString& path)
{
	m_proves = path;
	emit statementChanged(statementText());
}

void DiagramScene::setProvedBy(const QStringList& paths)
{
	m_provedBy = paths;
	emit statementChanged(statementText());
}

void DiagramScene::setDefines(const QString& term)
{
	m_defines = term;
	emit statementChanged(statementText());
}

void DiagramScene::layOutAfterRule(const QList<Node*>& made)
{
	// Guarded pointers: between here and the queued call the rule still has
	// its deletions to do, and one of these may be carried off by them.
	QList<QPointer<Node>> drawn;
	drawn.reserve(made.size());
	for (Node* node : made)
		drawn << QPointer<Node>(node);

	// Queued: a rule draws its conclusion in and then takes its deletions out,
	// and the tidy-up wants the diagram as it ends up, not half way through.
	QMetaObject::invokeMethod(this, [this, drawn] {
		QList<Node*> fresh;
		for (const QPointer<Node>& node : drawn)
			if (!node.isNull())
				fresh << node.data();
		layOut(QStringLiteral("grid"), fresh);
	}, Qt::QueuedConnection);
}

void DiagramScene::recordRuleApplication(const QString& description, const QList<Node*>& made,
                                         const QString& rulePath, const QString& ruleName,
                                         const QStringList& variables, const QStringList& values)
{
	if (m_history == nullptr)
		return;
	// A rule that draws nothing still happened: the step is the rule, not what
	// it left behind. (A recogniser rule - nothing dotted, nothing crossed out
	// - says the context is there, and that is the whole of its content.)
	m_history->record(new RuleApplied(description, made, rulePath, ruleName, variables, values));
	if (!made.isEmpty())
		emit nodesAdded(made);
	emit statementChanged(statementText());
	// a rule draws where the RULE was drawn, not where this diagram wants it:
	// what it made is tidied into place, and what was here already is not
	// rearranged around it
	layOutAfterRule(made);
}

QList<DiagramScene::ProofStep> DiagramScene::proofSteps() const
{
	QList<ProofStep> steps;
	if (m_history == nullptr)
		return steps;

	// only what is in force: anything undone is not part of how we got here
	const QList<Memento*>& all = m_history->mementos();
	for (int i = 0; i < m_history->position() && i < all.size(); ++i)
	{
		Memento* memento = all.at(i);
		if (memento == nullptr || memento->isPureGraphical())
			continue;

		if (auto* applied = dynamic_cast<RuleApplied*>(memento))
		{
			steps.append(ProofStep{ applied->rulePath(), applied->ruleName(), applied->describe(),
			                        applied->variables(), applied->values() });
		}
		else if (memento->typeTag() == MementoTag::RuleApplied)
		{
			// read back from a file: the rule and its bindings are the first
			// four fields of the payload, ahead of what it drew in
			ProofStep step;
			QByteArray bytes = memento->payload();
			QDataStream in(&bytes, QIODevice::ReadOnly);
			in.setVersion(QDataStream::Qt_6_0);
			in >> step.rulePath >> step.ruleName >> step.variables >> step.values;
			if (in.status() != QDataStream::Ok)
				continue;
			step.description = memento->describe();
			steps.append(step);
		}
		else if (memento->typeTag() == MementoTag::Note)
		{
			// a step of plain reasoning, which is still a step
			steps.append(ProofStep{ QString(), QString(), memento->describe(), {}, {} });
		}
	}
	return steps;
}

QList<DiagramScene::Component> DiagramScene::components() const
{
	QList<Component> found;
	if (m_ambientCategory == nullptr)
		return found;

	// the objects drawn here, and the arrows between them
	QList<Node*> objects;
	QList<Arrow*> arrows;
	for (QGraphicsItem* child : m_ambientCategory->childItems())
	{
		auto* node = dynamic_cast<Node*>(child);
		if (node == nullptr)
			continue;   // a label
		if (auto* arrow = dynamic_cast<Arrow*>(node))
			arrows << arrow;
		else
			objects << node;
	}

	// joined by an arrow, whichever way it points
	QHash<Node*, QList<Node*>> beside;
	for (Arrow* arrow : arrows)
	{
		Node* from = arrow->domain();
		Node* to = arrow->codomain();
		if (from == nullptr || to == nullptr)
			continue;
		beside[from].append(to);
		beside[to].append(from);
	}

	QSet<Node*> seen;
	for (Node* start : objects)
	{
		if (seen.contains(start))
			continue;

		// walk out from it as far as the arrows go
		Component piece;
		QList<Node*> queue{ start };
		seen.insert(start);
		while (!queue.isEmpty())
		{
			Node* at = queue.takeFirst();
			piece.objects << at;
			for (Node* next : beside.value(at))
				if (!seen.contains(next))
				{
					seen.insert(next);
					queue << next;
				}
		}

		// the arrows with both ends in this piece
		for (Arrow* arrow : arrows)
			if (piece.objects.contains(arrow->domain()) && piece.objects.contains(arrow->codomain()))
				piece.arrows << arrow;

		// what it is called, and where to look for it
		QStringList names;
		for (Node* node : piece.objects)
		{
			if (!node->id().isEmpty())
				names << node->id();
			piece.bounds |= node->sceneBoundingRect();
		}
		for (Arrow* arrow : piece.arrows)
			piece.bounds |= arrow->sceneBoundingRect();
		piece.title = names.isEmpty() ? QStringLiteral("(unnamed)") : names.join(", ");

		// the piece claims what its members claim; they are set together
		if (!piece.objects.isEmpty())
		{
			Node* first = piece.objects.first();
			piece.commutes = first->commutesInComponent();
			piece.rowsExact = first->rowsExactInComponent();
			piece.columnsExact = first->columnsExactInComponent();
		}
		found << piece;
	}
	return found;
}

void DiagramScene::layOut(const QString& kindId, const QList<Node*>& fresh)
{
	if (m_ambientCategory == nullptr)
		return;
	// not called `thread`: QObject has a thread() of its own, and a local of
	// that name reads as a call to it at a glance
	GraphLayoutThread* worker = GraphLayouts::make(kindId, nullptr);
	if (worker == nullptr)
		return;

	// The one moment the live diagram is read. Parents come before children,
	// so a node's parent index is always already known.
	QList<QPointer<Node>> order;
	LayoutGraph graph;
	graph.gridUnit = Node::snapEnabled() ? Node::snapUnit() : 0.0;

	QList<Node*> walk;
	walk << m_ambientCategory;
	QHash<Node*, int> indexOf;
	indexOf.insert(m_ambientCategory, 0);
	for (int at = 0; at < walk.size(); ++at)
		for (QGraphicsItem* child : walk.at(at)->childItems())
			if (auto* node = dynamic_cast<Node*>(child))
			{
				indexOf.insert(node, int(walk.size()));
				walk << node;
			}

	const QSet<Node*> justDrawn(fresh.constBegin(), fresh.constEnd());

	graph.nodes.reserve(walk.size());
	for (int i = 0; i < walk.size(); ++i)
	{
		Node* node = walk.at(i);
		LayoutNode record;
		// asked for by hand (the Layout menu) means nothing is settled: tidy
		// the lot. After a rule, only what the rule drew is unsettled.
		record.settled = !fresh.isEmpty() && !justDrawn.contains(node);
		record.parent = i == 0 ? -1 : indexOf.value(dynamic_cast<Node*>(node->parentItem()), -1);
		record.pos = node->pos();
		record.box = node->boxRect();
		if (auto* arrow = dynamic_cast<Arrow*>(node))
		{
			record.isArrow = true;
			record.domain = indexOf.value(arrow->domain(), -1);
			record.codomain = indexOf.value(arrow->codomain(), -1);
		}
		graph.nodes << record;
		order << QPointer<Node>(node);
	}

	connect(worker, &GraphLayoutThread::laidOut, this,
	        [this, order](const LayoutPlaces& places, const LayoutBends& bends) {
		QList<ItemsMoved::Move> moves;
		for (auto it = places.constBegin(); it != places.constEnd(); ++it)
		{
			if (it.key() < 0 || it.key() >= order.size())
				continue;
			Node* node = order.at(it.key()).data();
			// gone while the thread was thinking: leave it out rather than
			// putting a hole in the step
			if (node == nullptr || node->pos() == it.value())
				continue;
			moves.append(ItemsMoved::Move{ QPointer<Node>(node), node->pos(), it.value() });
		}

		// The shapes the layout asked for, in the arrows' own coordinates:
		// what came back is in the PARENT's frame, the one the positions are
		// in, and an arrow's bends are read in its own.
		struct Bending { QPointer<Arrow> arrow; QList<QPointF> before, after; };
		QList<Bending> bendings;
		for (auto it = bends.constBegin(); it != bends.constEnd(); ++it)
		{
			if (it.key() < 0 || it.key() >= order.size())
				continue;
			auto* arrow = dynamic_cast<Arrow*>(order.at(it.key()).data());
			if (arrow == nullptr)
				continue;
			QList<QPointF> want;
			want.reserve(it.value().size());
			for (const QPointF& point : it.value())
				want << arrow->mapFromParent(point);
			if (want == arrow->bends())
				continue;
			bendings.append(Bending{ QPointer<Arrow>(arrow), arrow->bends(), want });
		}

		if (moves.isEmpty() && bendings.isEmpty())
		{
			emit message(QStringLiteral("Already as tidy as that will make it."));
			return;
		}
		for (const ItemsMoved::Move& move : moves)
			if (!move.node.isNull())
				move.node->setPos(move.after);
		// after the moves: an arrow is bowed off the line between where its
		// ends have ENDED UP
		for (const Bending& bending : bendings)
			if (!bending.arrow.isNull())
				bending.arrow->setBends(bending.after);
		if (m_ambientCategory != nullptr)
		{
			m_ambientCategory->refreshDepthAppearance();
			m_ambientCategory->refreshFrame();
		}
		if (m_history != nullptr)
		{
			if (!moves.isEmpty())
				m_history->record(new ItemsMoved(QString("Tidied %1 items").arg(moves.size()), moves));
			for (const Bending& bending : bendings)
				if (!bending.arrow.isNull())
					m_history->record(new ArrowBent(QString("Bowed %1 clear").arg(bending.arrow->id()),
					                                bending.arrow.data(), bending.before, bending.after));
		}
	});
	connect(worker, &QThread::finished, worker, &QObject::deleteLater);
	worker->layOut(graph);
}

void DiagramScene::recordNote(const QString& text)
{
	if (m_history != nullptr && !text.isEmpty())
		m_history->record(new Note(text));
}


QPointF DiagramScene::inParentOf(const Node* node, const QPointF& scenePos)
{
	// A NODE WITH NO PARENT IS STILL SOMEWHERE.
	//
	// A child's pos() is in its parent's coordinates; a node hanging off the
	// scene itself has no parent to ask, and for that one the scene IS the
	// frame - pos() and scenePos() are the same numbers.
	//
	// Both halves of the move used to do this conversion inside `if (parent)`,
	// which quietly left a parentless node out: the grab offset was never
	// worked out and setPos was never called, so the node could be pressed,
	// picked up and dragged and simply did not go anywhere. That is every
	// top-level node there is - the two boxes of the classical view among them.
	if (node == nullptr)
		return scenePos;
	if (QGraphicsItem* parent = node->parentItem())
		return parent->mapFromScene(scenePos);
	return scenePos;
}

void DiagramScene::beginPress(Node* node, const QPointF& scenePos, Gesture gesture)
{
	if (node == nullptr || gesture == Gesture::None)
		return;
	m_pressed = node;
	m_pressScenePos = scenePos;
	m_gesture = gesture;
	m_moved = false;
	if (gesture == Gesture::Move)
	{
		m_moveFrom = node->pos();
		m_moveGrab = node->pos() - inParentOf(node, scenePos);
		// the cursor says what the drag is, for as long as it lasts
		QApplication::setOverrideCursor(Qt::SizeAllCursor);
	}
}

void DiagramScene::cancelMove()
{
	if (!isMoving())
		return;
	Node* node = m_pressed.data();
	m_pressed = nullptr;
	m_gesture = Gesture::None;
	QApplication::restoreOverrideCursor();
	node->setPos(m_moveFrom);
	emit message(QString("%1 put back.").arg(node->id()));
}
// ---------------------------------------------------------------- rules

namespace
{
	void everyNode(QGraphicsItem* parent, QList<Node*>& out)
	{
		for (QGraphicsItem* child : parent->childItems())
			if (auto* node = dynamic_cast<Node*>(child))
			{
				out << node;
				everyNode(node, out);
			}
	}
}

QString DiagramScene::ruleName() const
{
	return m_rule != nullptr ? m_rule->name() : QString();
}

bool DiagramScene::beginRule(const QString& path)
{
	endRule();
	auto rule = std::make_unique<Rule>(path);
	if (!rule->isValid())
	{
		emit message(QString("%1 could not be read as a rule.").arg(path));
		return false;
	}
	// Every diagram is a rule. One with nothing dotted and nothing crossed out
	// neither adds nor takes away: it says that this scene is THERE, and
	// applying it is citing it - the step a proof is made of.
	m_rule = std::move(rule);
	refreshRuleOverlay();
	return !m_matches.isEmpty();
}

void DiagramScene::endRule()
{
	if (m_rule == nullptr)
		return;
	clearRuleOverlay();
	m_rule.reset();
	m_matches.clear();
	emit ruleChanged(QString(), 0);
}

void DiagramScene::clearRuleOverlay()
{
	clearMatch();
}

void DiagramScene::clearMatch()
{
	// everything back to itself
	if (m_ambientCategory == nullptr)
		return;
	QList<Node*> nodes;
	everyNode(m_ambientCategory, nodes);
	nodes << m_ambientCategory;
	for (Node* node : nodes)
	{
		node->setOpacity(1.0);
		node->setFlag(QGraphicsItem::ItemIgnoresParentOpacity, false);
		node->setHighlight(false);
	}
}

void DiagramScene::showMatch(const QList<Node*>& matched)
{
	clearMatch();
	if (m_ambientCategory == nullptr || matched.isEmpty())
		return;

	// What the rule stands on is lit; the rest steps back. Each node keeps its
	// own opacity rather than its parent's, so a lit object inside a dimmed one
	// stays lit.
	const QSet<Node*> lit(matched.constBegin(), matched.constEnd());
	QList<Node*> nodes;
	everyNode(m_ambientCategory, nodes);
	for (Node* node : nodes)
	{
		node->setFlag(QGraphicsItem::ItemIgnoresParentOpacity, true);
		const bool on = lit.contains(node);
		node->setOpacity(on ? 1.0 : 0.22);
		node->setHighlight(on);
	}
	m_ambientCategory->setOpacity(1.0);   // the canvas itself stays as it is
}

void DiagramScene::refreshRuleOverlay()
{
	clearRuleOverlay();
	if (m_rule == nullptr)
		return;

	m_matches = RuleMatcher::find(*m_rule, this);

	QList<Node*> lit;
	for (const RuleMatch& match : m_matches)
	{
		for (Node* node : match.objects)
			lit << node;
		for (Arrow* arrow : match.arrows)
			lit << arrow;
	}
	showMatch(lit);

	emit ruleChanged(m_rule->name(), m_matches.size());
	emit message(m_matches.isEmpty()
		? QString("%1 fits nowhere in this diagram.").arg(m_rule->name())
		: QString("%1 fits in %2 place%3. Press Apply at one, or apply to all.")
			.arg(m_rule->name()).arg(m_matches.size()).arg(m_matches.size() == 1 ? "" : "s"));
}

void DiagramScene::applyMatch(int index)
{
	if (m_rule == nullptr || index < 0 || index >= m_matches.size())
		return;
	const bool recognises = m_rule->isRecogniser();
	const QList<Node*> made = RuleMatcher::apply(*m_rule, m_matches.at(index), this);
	if (recognises)
		emit message(QString("%1 fits here: the scene it describes is in the diagram, and the step says so.")
			.arg(m_rule->name()));
	else
		emit message(made.isEmpty()
			? QString("%1 applied there.").arg(m_rule->name())
			: QString("%1 applied: %2 thing%3 drawn in.").arg(m_rule->name()).arg(made.size()).arg(made.size() == 1 ? "" : "s"));
	// the diagram has changed: find it again, and light up what is left
	refreshRuleOverlay();
}

void DiagramScene::applyAllMatches()
{
	if (m_rule == nullptr)
		return;

	// A rule that only ADDS leaves the other places it fits exactly as they
	// were, so the matches found a moment ago are all still good. One that
	// TAKES AWAY does not: a match may share an arrow with the one just
	// applied, and applying it again would reach for something that is no
	// longer there. So a deleting rule is applied one place at a time, looking
	// again after each - bounded by the number of places it fitted at the
	// start, so a rule that keeps finding new work cannot run away with us.
	int places = 0;
	int drawn = 0;
	if (m_rule->deletes())
	{
		for (int guard = m_matches.size(); guard > 0 && !m_matches.isEmpty(); --guard)
		{
			drawn += RuleMatcher::apply(*m_rule, m_matches.first(), this).size();
			++places;
			m_matches = RuleMatcher::find(*m_rule, this);
		}
	}
	else
	{
		const QList<RuleMatch> pending = m_matches;
		for (const RuleMatch& match : pending)
			drawn += RuleMatcher::apply(*m_rule, match, this).size();
		places = pending.size();
	}

	emit message(QString("%1 applied in %2 place%3: %4 thing%5 drawn in.")
		.arg(m_rule->name()).arg(places).arg(places == 1 ? "" : "s")
		.arg(drawn).arg(drawn == 1 ? "" : "s"));
	refreshRuleOverlay();
}

// ---------------------------------------------------------------- the clipboard

QList<Node*> DiagramScene::selectedNodes() const
{
	QList<Node*> nodes;
	for (QGraphicsItem* item : selectedItems())
		if (auto* node = dynamic_cast<Node*>(item); node != nullptr && node != m_ambientCategory)
			nodes << node;
	return nodes;
}

void DiagramScene::selectOnly(const QList<Node*>& nodes)
{
	clearSelection();
	for (Node* node : nodes)
		if (node != nullptr)
			node->setSelected(true);
}

void DiagramScene::selectEverything()
{
	QList<Node*> everything;
	if (m_ambientCategory != nullptr)
		everyNode(m_ambientCategory, everything);
	selectOnly(everything);
	emit message(QString("Selected %1 thing%2.").arg(everything.size()).arg(everything.size() == 1 ? "" : "s"));
}

Category* DiagramScene::categoryForDrop(const QPointF& scenePos) const
{
	// the category the point is IN: the one hit, or the one holding whatever
	// was hit, or - failing both - the canvas itself
	if (QGraphicsItem* item = hitItem(scenePos))
	{
		if (auto* category = dynamic_cast<Category*>(item))
			return category;
		if (auto* node = dynamic_cast<Node*>(item); node != nullptr)
			if (Category* home = node->surroundingCategory())
				return home;
		// a label: its node's home
		if (auto* node = dynamic_cast<Node*>(item->parentItem()); node != nullptr)
			if (Category* home = node->surroundingCategory())
				return home;
	}
	return m_ambientCategory;
}

bool DiagramScene::clipboardHasFragment()
{
	const QMimeData* data = QApplication::clipboard()->mimeData();
	return data != nullptr && data->hasFormat(SceneFile::fragmentMimeType());
}

bool DiagramScene::copySelection()
{
	const QList<Node*> chosen = selectedNodes();
	if (chosen.isEmpty())
	{
		emit message("Nothing is selected to copy.");
		return false;
	}
	const QByteArray payload = SceneFile::copyFragment(chosen);
	if (payload.isEmpty())
		return false;

	auto* data = new QMimeData();
	data->setData(SceneFile::fragmentMimeType(), payload);
	// beside it, what it SAYS - so a selection pasted into a document arrives
	// as a sentence rather than as nothing at all
	data->setText(Translation::asPlainText(Translation::describe(this, chosen)));
	QApplication::clipboard()->setMimeData(data);

	emit message(QString("Copied %1 thing%2. Ctrl+V puts it down; Ctrl and drag carries it.")
		.arg(chosen.size()).arg(chosen.size() == 1 ? "" : "s"));
	return true;
}

bool DiagramScene::cutSelection()
{
	const QList<Node*> chosen = selectedNodes();
	if (!copySelection())
		return false;
	deleteNodes(chosen);
	return true;
}

QList<Node*> DiagramScene::paste()
{
	return paste(m_lastScenePos);
}

QList<Node*> DiagramScene::paste(const QPointF& scenePos)
{
	const QMimeData* data = QApplication::clipboard()->mimeData();
	if (data == nullptr || !data->hasFormat(SceneFile::fragmentMimeType()))
	{
		emit message("There is no piece of a diagram on the clipboard.");
		return QList<Node*>();
	}
	return dropFragment(data->data(SceneFile::fragmentMimeType()), scenePos);
}

QList<Node*> DiagramScene::dropFragment(const QByteArray& payload, const QPointF& scenePos)
{
	Category* into = categoryForDrop(scenePos);
	if (into == nullptr)
		return QList<Node*>();

	int dropped = 0;
	const QList<Node*> made = SceneFile::pasteFragment(payload, into, scenePos, &dropped);
	if (made.isEmpty())
	{
		emit message("Nothing of that could be put down here.");
		return made;
	}

	recordCreation(QString("Pasted %1 thing%2 into %3")
		.arg(made.size()).arg(made.size() == 1 ? "" : "s", into->id()), made);
	selectOnly(made);
	checkDiagram();
	emit message(dropped == 0
		? QString("Put down %1 thing%2 in %3.").arg(made.size()).arg(made.size() == 1 ? "" : "s", into->id())
		: QString("Put down %1 thing%2 in %3. %4 arrow%5 left behind: an end of it did not come along.")
			.arg(made.size()).arg(made.size() == 1 ? "" : "s", into->id())
			.arg(dropped).arg(dropped == 1 ? " was" : "s were"));
	return made;
}

QList<Node*> DiagramScene::duplicateSelection()
{
	const QList<Node*> chosen = selectedNodes();
	if (chosen.isEmpty())
	{
		emit message("Nothing is selected to copy.");
		return QList<Node*>();
	}
	const QByteArray payload = SceneFile::copyFragment(chosen);
	if (payload.isEmpty())
		return QList<Node*>();

	// a little down and to the right of what it is a copy of, so the two are
	// not one on top of the other
	QRectF bounds;
	for (Node* node : chosen)
		bounds |= node->sceneBoundingRect();
	const qreal step = qMax(20.0, Node::snapEnabled() ? Node::snapUnit() : 25.0);
	Category* into = chosen.first()->surroundingCategory();
	if (into == nullptr)
		into = m_ambientCategory;

	int dropped = 0;
	const QList<Node*> made = SceneFile::pasteFragment(
		payload, into, bounds.topLeft() + QPointF(step, step), &dropped);
	if (made.isEmpty())
		return made;
	recordCreation(QString("Copied %1 thing%2").arg(made.size()).arg(made.size() == 1 ? "" : "s"), made);
	selectOnly(made);
	checkDiagram();
	emit message(QString("Copied %1 thing%2.").arg(made.size()).arg(made.size() == 1 ? "" : "s"));
	return made;
}
