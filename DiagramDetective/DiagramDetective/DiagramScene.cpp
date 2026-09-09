#include "DiagramScene.h"
#include "categories/BuiltInCategories.h"
#include "TutorSession.h"
#include "AppSettings.h"
#include "NodeHandles.h"
#include "NodeLabel.h"
#include "ArrowTutor.h"
#include "history/SceneHistory.h"
#include "history/Mementos.h"
#include "Arrow.h"
#include "props/MapsElements.h"
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
#include <functional>

DiagramScene::DiagramScene(QObject* parent)
	: QGraphicsScene(parent)
{
	// a FIXED scene rect: without one the rect follows the items, and the view
	// re-centres the scene whenever it changes; the first node placed would
	// jump away from the cursor
	setSceneRect(-4000, -4000, 8000, 8000);
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
}

void DiagramScene::setAmbientCategory(const QString& name)
{
	if (m_ambientCategory != nullptr && m_ambientCategory->id() == name)
		return;

	// Whatever is pointing into the old canvas - a tutor's arrow, the handle
	// bar, a half-drawn arrow - has to let go before it is taken away.
	if (!m_session.isNull())
		m_session->cancel();
	hideHandles();
	hideArrowPreview();

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
	if (dynamic_cast<Node*>(item) == nullptr)
		return dynamic_cast<Category*>(item->parentItem());
	return nullptr;   // some other node: its own business
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
		m_ambientCategory->popupContextMenu(event->screenPos());
		event->accept();
	}
}

void DiagramScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
	// a double-click ON A LABEL puts the keyboard in it rather than placing
	// anything; while it is already being edited the text widget wants the
	// double-click itself, to select a word
	if (auto* label = dynamic_cast<NodeLabel*>(hitItem(event->scenePos())))
	{
		if (!label->isEditing())
		{
			label->beginEdit();
			event->accept();
			return;
		}
		QGraphicsScene::mouseDoubleClickEvent(event);
		return;
	}

	// A double-click on an ARROW shows or hides what its mapping drew. The
	// image nodes are only hidden, so whatever has been drawn inside them is
	// there again when it comes back.
	if (auto* arrow = dynamic_cast<Arrow*>(nodeAt(event->scenePos())))
	{
		if (auto* maps = dynamic_cast<MapsElements*>(arrow->prop(MapsElements::Key())))
		{
			const bool show = !maps->imagesVisible();
			maps->setImagesVisible(show);
			auto* cod = maps->codomain();
			emit message(show
				? QString("The image of %1 is back in %2.").arg(arrow->id(), cod != nullptr ? cod->id() : QString())
				: QString("The image of %1 is put away. Double-click it again to bring it back.").arg(arrow->id()));
			event->accept();
			return;
		}
	}

	// let items under the cursor take the double-click first; a double-click
	// on the canvas creates an object
	QGraphicsScene::mouseDoubleClickEvent(event);

	// a double-click is not the second click of an arrow
	if (arrowPending())
		cancelArrow();

	Category* category = categoryAt(hitItem(event->scenePos()));
	if (category == nullptr)
		return;

	// the category decides what it is made of: a category in BigCat, a set in Set, ...
	Object* placed = category->createCanvasObject(event->scenePos());
	if (placed != nullptr)
		recordCreation(QString("Placed %1 in %2").arg(placed->id(), category->id()), { placed });
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
		if (item == m_handle || item == m_preview)
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

void DiagramScene::showHandles(Node* node, const QPointF& itemPos)
{
	if (m_handle == nullptr)
	{
		m_handle = new NodeHandles();
		addItem(m_handle);
		connect(m_handle, &NodeHandles::arrowRequested, this, &DiagramScene::beginArrow);
		connect(m_handle, &NodeHandles::deleteRequested, this, &DiagramScene::deleteNode);
		connect(m_handle, &NodeHandles::chaseRequested, this, [this](Node* node) {
			auto* arrow = dynamic_cast<Arrow*>(node);
			auto* maps = arrow != nullptr
				? dynamic_cast<MapsElements*>(arrow->prop(MapsElements::Key())) : nullptr;
			if (maps == nullptr || maps->codomain() == nullptr)
				return;
			// carried across, shown, and KEPT carried: from here the codomain
			// follows the domain
			maps->setImagesVisible(true);
			if (maps->isLive())
				maps->sync();
			else
				maps->setLive(true);
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
	{
		QList<Node*> everything;
		collectNamed(m_ambientCategory, everything);
		for (Node* node : everything)
			if (auto* home = dynamic_cast<Category*>(node); home != nullptr && home->holdsAnything())
			{
				if (home->rowsExact()) exactRows << QString("the diagram in %1").arg(home->id());
				if (home->columnsExact()) exactColumns << QString("the diagram in %1").arg(home->id());
			}
	}
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
	if (m_preview == nullptr)
	{
		m_preview = new QGraphicsLineItem();
		m_preview->setPen(QPen(QColor(99, 102, 241), 1.5, Qt::DashLine));
		m_preview->setZValue(9999);
		m_preview->setAcceptedMouseButtons(Qt::NoButton);   // it is a hint, not a target
		addItem(m_preview);
	}
	const QPointF c = from->sceneBoundingRect().center();
	m_preview->setLine(QLineF(c, c));
	m_preview->show();
}

void DiagramScene::hideArrowPreview()
{
	m_arrowFrom = nullptr;
	if (m_preview != nullptr)
		m_preview->hide();
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
			Node* node = nodeAt(event->scenePos());
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

		// remember where everything that could be dragged is standing, so the
		// move can be recorded as one change when the button comes back up
		m_dragFrom.clear();
		QList<Node*> watched;
		if (Node* node = nodeAt(event->scenePos()))
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
	// a move ends where the button comes up, as one change in the history
	if (isMoving())
	{
		Node* node = m_pressed.data();
		m_pressed = nullptr;
		m_gesture = Gesture::None;
		QApplication::restoreOverrideCursor();
		if (m_moved && node->pos() != m_moveFrom && m_history != nullptr)
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
	if (arrowPending() && m_preview != nullptr)
		m_preview->setLine(QLineF(m_arrowFrom->sceneBoundingRect().center(), event->scenePos()));

	if (!m_pressed.isNull() && m_gesture != Gesture::None)
	{
		const bool gone = QLineF(m_pressScenePos, event->scenePos()).length() > 6.0;

		// by the label: the object goes where the mouse goes, on the grid
		if (m_gesture == Gesture::Move)
		{
			if (gone || m_moved)
			{
				m_moved = true;
				if (QGraphicsItem* parent = m_pressed->parentItem())
					m_pressed->setPos(parent->mapFromScene(event->scenePos()) + m_moveGrab);
			}
			event->accept();
			return;
		}

		// by the body: an arrow being pulled out of it
		if (gone)
		{
			Node* from = m_pressed.data();
			m_pressed = nullptr;
			m_gesture = Gesture::None;
			beginArrow(from);
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
	QGraphicsScene::keyPressEvent(event);
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

void DiagramScene::recordNote(const QString& text)
{
	if (m_history != nullptr && !text.isEmpty())
		m_history->record(new Note(text));
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
		if (QGraphicsItem* parent = node->parentItem())
			m_moveGrab = node->pos() - parent->mapFromScene(scenePos);
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
	// The button that sits under a place the rule fits. A scene-level item at
	// screen size, like the handle bar; a click applies the rule there.
	class ApplyButton : public QGraphicsObject
	{
	public:
		explicit ApplyButton(const QString& text)
			: m_text(text)
		{
			setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
			setZValue(10001);
			setAcceptHoverEvents(true);
			setCursor(QCursor(Qt::PointingHandCursor));

			// as wide as its words need, with room either side; a very long
			// name is shortened with an ellipsis rather than cut off
			m_font.setBold(true);
			m_font.setPointSizeF(9.5);
			const QFontMetricsF metrics(m_font);
			const qreal most = 260;
			if (metrics.horizontalAdvance(m_text) > most)
				m_text = metrics.elidedText(m_text, Qt::ElideRight, most);
			m_width = metrics.horizontalAdvance(m_text) + 28;
		}

		std::function<void()> onClick;

		QRectF boundingRect() const override { return QRectF(-m_width / 2, -14, m_width, 28); }
		QPainterPath shape() const override
		{
			QPainterPath path;
			path.addRoundedRect(boundingRect().adjusted(2, 2, -2, -2), 12, 12);
			return path;
		}
		void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
		{
			painter->setRenderHint(QPainter::Antialiasing, true);
			painter->setPen(QPen(QColor(255, 255, 255, 230), 1.2));
			painter->setBrush(m_hover ? QColor(21, 128, 61) : QColor(34, 197, 94, 240));
			painter->drawRoundedRect(boundingRect().adjusted(2, 2, -2, -2), 12, 12);
			painter->setPen(Qt::white);
			painter->setFont(m_font);
			painter->drawText(boundingRect(), Qt::AlignCenter, m_text);
		}

	protected:
		void mousePressEvent(QGraphicsSceneMouseEvent* event) override
		{
			if (event->button() == Qt::LeftButton)
			{
				event->accept();
				if (onClick)
					onClick();
				return;
			}
			QGraphicsObject::mousePressEvent(event);
		}
		void hoverEnterEvent(QGraphicsSceneHoverEvent*) override { m_hover = true; update(); }
		void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override { m_hover = false; update(); }

	private:
		QString m_text;
		QFont m_font;
		qreal m_width = 92;
		bool m_hover = false;
	};

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
	if (!rule->hasConclusion())
	{
		emit message(QString("%1 concludes nothing - nothing in it is drawn dotted - so there is nothing "
		                     "to apply.").arg(rule->name()));
		return false;
	}
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
	for (QGraphicsObject* button : m_applyButtons)
	{
		removeItem(button);
		delete button;
	}
	m_applyButtons.clear();

	// everything back to itself
	if (m_ambientCategory != nullptr)
	{
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
}

void DiagramScene::refreshRuleOverlay()
{
	clearRuleOverlay();
	if (m_rule == nullptr)
		return;

	m_matches = RuleMatcher::find(*m_rule, this);

	if (m_ambientCategory != nullptr)
	{
		// what the rule fits is lit; the rest steps back. Each node keeps its
		// own opacity rather than its parent's, so a lit object inside a
		// dimmed one stays lit.
		QSet<Node*> lit;
		for (const RuleMatch& match : m_matches)
		{
			for (Node* node : match.objects)
				lit.insert(node);
			for (Arrow* arrow : match.arrows)
				lit.insert(arrow);
		}
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

	// a button under each place it fits
	for (int i = 0; i < m_matches.size(); ++i)
	{
		auto* button = new ApplyButton(QString("Apply %1").arg(m_rule->name()));
		const QRectF at = m_matches.at(i).bounds;
		button->setPos(at.center().x(), at.bottom() + 18);
		button->onClick = [this, i] { applyMatch(i); };
		addItem(button);
		m_applyButtons << button;
	}

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
	const QList<Node*> made = RuleMatcher::apply(*m_rule, m_matches.at(index), this);
	emit message(made.isEmpty()
		? QString("%1 made nothing there.").arg(m_rule->name())
		: QString("%1 applied: %2 thing%3 drawn in.").arg(m_rule->name()).arg(made.size()).arg(made.size() == 1 ? "" : "s"));
	// the diagram has changed: find it again, and light up what is left
	refreshRuleOverlay();
}

void DiagramScene::applyAllMatches()
{
	if (m_rule == nullptr)
		return;
	// the matches as they stand now: applying one never removes anything, so
	// the others stay good, and what a new one might match is not chased
	const QList<RuleMatch> pending = m_matches;
	int drawn = 0;
	for (const RuleMatch& match : pending)
		drawn += RuleMatcher::apply(*m_rule, match, this).size();
	emit message(QString("%1 applied in %2 place%3: %4 thing%5 drawn in.")
		.arg(m_rule->name()).arg(pending.size()).arg(pending.size() == 1 ? "" : "s")
		.arg(drawn).arg(drawn == 1 ? "" : "s"));
	refreshRuleOverlay();
}
