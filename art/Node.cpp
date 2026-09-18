#include "art/Node.h"
#include "art/Category.h"
#include "art/Arrow.h"

#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QWidgetAction>
#include <QToolButton>
#include <QGridLayout>
#include <QColorDialog>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsScene>
#include <QApplication>
#include "art/DiagramScene.h"
#include "core/AppSettings.h"
#include "core/Emoji.h"
#include "core/Notation.h"
#include "core/props/MapsElements.h"
#include "core/history/SceneHistory.h"
#include "core/history/Mementos.h"
#include <QtMath>
#include <algorithm>

qreal Node::s_snapUnit = 25.0;
bool Node::s_snapEnabled = true;

bool Node::s_collisionEnabled = true;
bool Node::s_userDragging = false;
qreal Node::s_pushEpsilon = 1.0;
int Node::s_pushDepth = 0;
QSet<Node*> Node::s_pushed;

namespace
{
	// How far `pushed` has to travel along `direction` to be clear of
	// `pusher`. Moving along the direction closes the overlap on x at the rate
	// |direction.x| and on y at |direction.y|; whichever axis clears first is
	// the answer.
	qreal separation(const QRectF& pusher, const QRectF& pushed, const QPointF& direction)
	{
		const QRectF overlap = pusher.intersected(pushed);
		if (overlap.isEmpty())
			return 0;
		qreal distance = -1;
		if (qAbs(direction.x()) > 1e-6)
			distance = overlap.width() / qAbs(direction.x());
		if (qAbs(direction.y()) > 1e-6)
		{
			const qreal alongY = overlap.height() / qAbs(direction.y());
			distance = distance < 0 ? alongY : qMin(distance, alongY);
		}
		return distance < 0 ? 0 : distance;
	}
}

QPointF Node::snapped(const QPointF& parentPos) const
{
	if (!s_snapEnabled || s_snapUnit <= 0)
		return parentPos;
	// snap in scene coordinates: the parent's transform (its position,
	// nesting depth) must not shift the grid
	const QGraphicsItem* parent = parentItem();
	const QPointF scenePt = parent != nullptr ? parent->mapToScene(parentPos) : parentPos;
	const QPointF onGrid(qRound(scenePt.x() / s_snapUnit) * s_snapUnit,
	                     qRound(scenePt.y() / s_snapUnit) * s_snapUnit);
	return parent != nullptr ? parent->mapFromScene(onGrid) : onGrid;
}

quint64 Node::s_nextKey = 0;

Node::Node(const QString& id, QGraphicsItem *parent)
	: QGraphicsObject(parent)
	, m_key(QStringLiteral("n%1").arg(++s_nextKey))
{
	setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);   // itemChange sees moves
	setId(id);
}

void Node::setKey(const QString& key)
{
	if (key.isEmpty())
		return;   // an older file, with nothing to restore: keep the fresh one
	m_key = key;
	// Step the counter past anything read back, so a node made later in this
	// session cannot be handed a key a node read from a file already has.
	const quint64 was = QStringView(key).mid(1).toULongLong();
	if (was > s_nextKey)
		s_nextKey = was;
}

Node::~Node()
{
	// Announce it FIRST, while the label is still here: whoever is listening
	// (the image this node has under a functor, an arrow that ends on it) has
	// to be able to ask what this node was called in order to find its own
	// work. Deleting the label first would leave id() empty.
	emit deleted(this);

	if (m_idText != nullptr)
	{
		delete m_idText;
		m_idText = nullptr;
	}
}

void Node::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	painter->setRenderHint(QPainter::RenderHint::Antialiasing, true);
}

QString Node::id() const
{
	// the source, never the document: what is drawn has <sub> markup folded
	// into it and is not a name (see NodeLabel)
	return m_idText != nullptr ? m_idText->source() : QString();
}

void Node::setId(const QString& id) {
	// a label typed by hand (or set by anything but the formula itself) is no
	// longer built out of other labels
	if (!m_settingDerivedLabel && !m_labelPattern.isEmpty()
	 && (m_idText == nullptr || id != m_idText->source()))
		clearDerivedLabel();

	if (m_idText != nullptr)
	{
		if (m_idText->source() != id)
		{
			prepareGeometryChange();
			ancestorsPrepareGeometryChange();
			m_idText->setSource(id);
			placeLabel();
			ancestorsUpdate();
			emit idChanged(this, id);
		}
	}
	else {
		if (id.isEmpty())
			return;
		prepareGeometryChange();
		ancestorsPrepareGeometryChange();
		m_idText = new NodeLabel(id, this);
		placeLabel();
		ancestorsUpdate();
		emit idChanged(this, id);
	}
}

void Node::setDefaultLook(const QBrush& fill, const QPen& border)
{
	m_fill = fill;
	m_border = border;
	update();
}

void Node::setFill(const QBrush& fill)
{
	if (m_fill == fill) return;
	m_fill = fill;
	update();
	emit styleChanged(this);
}

bool Node::holdsAnything() const
{
	return containedCount() > 0;
}

QRectF Node::labelRect() const
{
	if (m_idText == nullptr)
		return QRectF();
	return QRectF(m_idText->pos(), m_idText->boundingRect().size());
}

void Node::setHighlight(bool on)
{
	if (m_highlighted == on)
		return;
	m_highlighted = on;
	update();
}

void Node::setCommutesInComponent(bool commutes)
{
	m_componentCommutes = commutes;
}

void Node::setError(bool error)
{
	if (m_inCycle == error)
		return;
	m_inCycle = error;
	update();
}

void Node::setCornerRadius(qreal radius)
{
	radius = qMax(0.0, radius);
	if (qFuzzyCompare(m_cornerRadius, radius))
		return;
	m_cornerRadius = radius;
	update();
	emit styleChanged(this);
}

void Node::setExistsSuch(bool existsSuch)
{
	if (m_existsSuch == existsSuch)
		return;
	m_existsSuch = existsSuch;
	if (m_existsSuch && m_deleteMark)
		m_deleteMark = false;   // claimed to exist and struck off is not a thing to be
	update();
	emit styleChanged(this);
}

void Node::setDeleteMark(bool marked)
{
	if (m_deleteMark == marked)
		return;
	m_deleteMark = marked;
	if (m_deleteMark && m_existsSuch)
		m_existsSuch = false;
	update();
	emit styleChanged(this);
}

void Node::setBorder(const QPen& border)
{
	if (m_border == border) return;
	prepareGeometryChange();   // a wider pen paints outside the old rect
	ancestorsPrepareGeometryChange();
	m_border = border;
	update();
	ancestorsUpdate();
	emit styleChanged(this);
}

QRectF Node::childFrame() const
{
	// Qt's childrenBoundingRect() asks every descendant for its boundingRect,
	// and it can be called WHILE ONE OF THEM IS BEING CONSTRUCTED: an item
	// built as `new Object(name, parent)` is put into its parent's child list
	// from inside QGraphicsItem's constructor, long before the Object part of
	// it exists, so its boundingRect is still the pure virtual one and the
	// process aborts.
	//
	// dynamic_cast is exactly the instrument for this: it can only see the
	// part of an object that has been built. A child we cannot yet name as a
	// Node or as a text label is not asked for its rect - it is left out of
	// our frame for the instant it takes to finish, and the deferred
	// refreshFrame() takes it in.
	QRectF frame;
	for (QGraphicsItem* child : childItems())
	{
		const bool ready = dynamic_cast<Node*>(child) != nullptr
		                || dynamic_cast<QGraphicsTextItem*>(child) != nullptr;
		if (!ready)
			continue;
		if (!child->isVisible())
			continue;   // put away: it does not hold the frame open
		frame |= child->mapRectToParent(child->boundingRect());
	}
	return frame;
}

namespace
{
	// the objects a node holds, in order: arrows are not part of the nesting
	QList<Node*> steppableChildren(const QGraphicsItem* parent)
	{
		QList<Node*> kids;
		for (QGraphicsItem* child : parent->childItems())
		{
			auto* node = dynamic_cast<Node*>(child);
			if (node != nullptr && dynamic_cast<Arrow*>(node) == nullptr)
				kids << node;
		}
		return kids;
	}
}

QList<int> Node::pathFromRoot() const
{
	QList<int> path;
	const QGraphicsItem* item = this;
	for (QGraphicsItem* parent = parentItem(); parent != nullptr; parent = parent->parentItem())
	{
		const int index = steppableChildren(parent).indexOf(const_cast<Node*>(dynamic_cast<const Node*>(item)));
		if (index >= 0)
			path.prepend(index);   // an arrow is not in that list: it shares its category's path
		item = parent;
	}
	return path;
}

Node* Node::fromPath(Node* root, const QList<int>& path)
{
	Node* node = root;
	for (int index : path)
	{
		if (node == nullptr)
			return nullptr;
		const QList<Node*> kids = steppableChildren(node);
		if (index < 0 || index >= kids.size())
			return nullptr;
		node = kids.at(index);
	}
	return node;
}

int Node::nesting() const
{
	int depth = 0;
	for (const QGraphicsItem* p = parentItem(); p != nullptr; p = p->parentItem())
		if (dynamic_cast<const Node*>(p) != nullptr)
			++depth;
	return depth;
}

void Node::refreshDepthAppearance()
{
	applyDepthAppearance(nesting());
	for (QGraphicsItem* child : childItems())
		if (auto* node = dynamic_cast<Node*>(child))
			node->refreshDepthAppearance();   // and so on, all the way down
}

void Node::applyDepthAppearance(int depth)
{
	if (label() == nullptr)
		return;
	// the objects drawn in the ambient category keep the normal size; every
	// level of nesting below that takes a little off
	qreal base = AppSettings::instance().labelPointSize();
	if (base <= 0)
		base = QApplication::font().pointSizeF();
	if (base <= 0)
		base = 9.0;
	const qreal size = qMax(5.0, base * qPow(0.88, qMax(0, depth - 1)));
	QFont f = labelFont();
	if (qFuzzyCompare(f.pointSizeF(), size))
		return;
	f.setPointSizeF(size);
	setLabelFont(f);
}

int Node::containedCount(const QGraphicsItem* except) const
{
	int n = 0;
	for (QGraphicsItem* c : childItems())
		if (c != except && dynamic_cast<Node*>(c) != nullptr)   // nodes only: never our own label
			++n;
	return n;
}

void Node::refreshLabelWeight(int contained)
{
	QFont f = labelFont();
	const bool bold = contained > 0;
	if (f.bold() == bold)
		return;
	f.setBold(bold);
	setLabelFont(f);
}

QFont Node::labelFont() const
{
	return m_idText != nullptr ? m_idText->font() : QFont();
}

void Node::setLabelFont(const QFont& font)
{
	if (m_idText == nullptr || m_idText->font() == font)
		return;
	prepareGeometryChange();
	ancestorsPrepareGeometryChange();
	m_idText->setFont(font);
	placeLabel();
	update();
	ancestorsUpdate();
}

void Node::placeLabel()
{
	centreLabel();
}

void Node::centreLabel()
{
	if (m_idText == nullptr)
		return;
	const QRectF r = m_idText->boundingRect();
	// centred on this node's origin, plus wherever the label has been dragged
	m_idText->setPos(QPointF(-r.width() / 2.0, -r.height() / 2.0) + m_labelOffset);
}

void Node::setLabelOffset(const QPointF& offset)
{
	if (m_labelOffset == offset)
		return;
	prepareGeometryChange();
	ancestorsPrepareGeometryChange();
	m_labelOffset = offset;
	placeLabel();
	rememberLabelBox();   // this is where it belongs against the frame as it stands
	update();
	ancestorsUpdate();
}

void Node::labelMoved(const QPointF& pos)
{
	if (m_idText == nullptr)
		return;
	// what the user dragged is remembered as a displacement from where the
	// label would otherwise sit, so it travels with the node
	const QRectF r = m_idText->boundingRect();
	prepareGeometryChange();
	ancestorsPrepareGeometryChange();
	m_labelOffset = pos - QPointF(-r.width() / 2.0, -r.height() / 2.0);
	rememberLabelBox();   // dragged here against the frame as it stands
	update();
	ancestorsUpdate();
}

void Node::labelDragFinished(const QPointF& fromPos)
{
	if (m_idText == nullptr)
		return;
	const QRectF r = m_idText->boundingRect();
	const QPointF centred(-r.width() / 2.0, -r.height() / 2.0);
	if (auto* diagram = diagramOf(this))
		diagram->history()->record(new LabelMoved(
			QString("Moved the label of %1").arg(id().isEmpty() ? QStringLiteral("a node") : id()),
			this, fromPos - centred, m_labelOffset));
}

bool Node::labelIsLocked() const
{
	// stamped with what it is the image of: only a functor puts that there
	return !data(MapsElements::ImageSourceKey).toString().isEmpty();
}

void Node::refreshLabelMovability()
{
	if (m_idText == nullptr)
		return;
	m_idText->setToolTip(labelIsLocked()
		? QStringLiteral("This name is made from the functor's name and the name of what it is the "
		                 "image of. Change it by renaming either of those.")
		: QString());
	const bool movable = labelIsMovable();
	if (((m_idText->flags() & QGraphicsItem::ItemIsMovable) != 0) == movable)
		return;
	m_idText->setFlag(QGraphicsItem::ItemIsMovable, movable);
	// a label that carries its node says so with the move cursor; one that is
	// only a name to be placed says so with the open hand
	m_idText->setCursor(movable ? Qt::OpenHandCursor : Qt::SizeAllCursor);
}

QRectF Node::contentFrame() const
{
	// The same care as childFrame(): a child still being constructed cannot be
	// asked for its rect.
	QRectF frame;
	for (QGraphicsItem* child : childItems())
	{
		if (dynamic_cast<Node*>(child) == nullptr)
			continue;   // our own label is not part of the box
		if (!child->isVisible())
			continue;
		frame |= child->mapRectToParent(child->boundingRect());
	}
	// nothing held: this node IS its label, so that is the whole of it
	return frame.isNull() ? childFrame() : frame;
}

QVariant Node::itemChange(GraphicsItemChange change, const QVariant& value)
{
	switch (change)
	{
	case ItemPositionChange:
		// about to move: every ancestor's frame is about to change shape
		ancestorsPrepareGeometryChange();
		// the position Qt will apply is the one we return: put it on the grid
		return QGraphicsObject::itemChange(change, snapped(value.toPointF()));
	case ItemPositionHasChanged:
	{
		ancestorsUpdate();
		const QPointF p = value.toPointF();
		const QPointF delta = p - m_lastPos;
		m_lastPos = p;
		emit moved(this, delta);
		pushSiblings(delta);
		break;
	}
	case ItemChildAddedChange:
	case ItemChildRemovedChange:
		// NOTHING that reads a bounding rect may happen here.
		//
		// Qt sends ItemChildAddedChange from inside setParentItemHelper, and
		// for a child built as `new Object(name, parentCategory)` that is
		// reached from the child's own QGraphicsObject CONSTRUCTOR: the child
		// is already in our childItems(), but only its QGraphicsItem base
		// exists, so its vtable entry for boundingRect() is still the pure
		// virtual one. Our frames are unions of our children
		// (childrenBoundingRect), so prepareGeometryChange() here walks
		// straight into that child and the process aborts on a pure virtual
		// call. The label item created in setId hits the same window.
		//
		// So we only note that the frame is stale, and put it right on the
		// next turn of the event loop, by which time the child is whole.
		scheduleFrameRefresh();
		break;
	default:
		break;
	}
	return QGraphicsObject::itemChange(change, value);
}

void Node::refreshFrame()
{
	prepareGeometryChange();
	ancestorsPrepareGeometryChange();
	// a node that holds other nodes shows its id in bold, and its label becomes
	// a thing that can be dragged clear of the box: both can change for us and
	// for anything we sit inside
	refreshLabelWeight(containedCount());
	refreshLabelMovability();
	for (QGraphicsItem* p = parentItem(); p != nullptr; p = p->parentItem())
		if (auto* node = dynamic_cast<Node*>(p))
		{
			node->refreshLabelWeight(node->containedCount());
			node->refreshLabelMovability();
		}
	update();
	ancestorsUpdate();
	settleLabelSoon();   // what we hold has changed, so our frame has a new shape
}

namespace
{
	// One axis of the label's centre, carried from the old frame to the new.
	// See the note on Node::settleLabel for what this is doing and why k comes
	// out the way it does.
	qreal follow(qreal c, qreal lo0, qreal hi0, qreal lo1, qreal hi1)
	{
		const qreal span = hi0 - lo0;
		// A frame with no width to speak of has no proportion to keep: carry
		// the gap over rather than dividing by nearly nothing.
		if (span <= 1e-6)
			return lo1 + (c - lo0);
		if (c <= lo0)
			return lo1 + (c - lo0);   // beyond the low edge: keep the gap, k = 1
		if (c >= hi0)
			return hi1 + (c - hi0);   // beyond the high edge: likewise
		// inside: the same fraction of the way across as before
		return lo1 + (c - lo0) / span * (hi1 - lo1);
	}
}

void Node::rememberLabelBox()
{
	const QRectF now = boxRect();
	m_labelBoxValid = labelFollowsBox() && !now.isEmpty();
	m_labelBox = m_labelBoxValid ? now : QRectF();
}

void Node::placeLabelAboveBox()
{
	if (m_idText == nullptr)
		return;
	const QRectF box = boxRect();
	if (box.isEmpty())
		return;

	// m_labelOffset is the label's CENTRE in our coordinates (see centreLabel),
	// so this puts its bottom edge a little clear of the frame's top and its
	// middle over the middle of the frame.
	const qreal gap = 4.0;
	const QRectF text = m_idText->boundingRect();
	prepareGeometryChange();
	ancestorsPrepareGeometryChange();
	m_labelOffset = QPointF(box.center().x(), box.top() - gap - text.height() / 2.0);
	placeLabel();
	update();
	ancestorsUpdate();
}

void Node::settleLabelSoon()
{
	if (m_labelSettleQueued)
		return;
	m_labelSettleQueued = true;
	// queued on us: if we are destroyed first, the call is dropped with our
	// other posted events
	QMetaObject::invokeMethod(this, [this] { settleLabel(); }, Qt::QueuedConnection);
}

void Node::settleLabel()
{
	m_labelSettleQueued = false;
	if (m_idText == nullptr || !labelFollowsBox())
	{
		// no frame to follow - a leaf object IS its label. If it ever gains
		// one, that frame will be the one the label is measured against.
		m_labelBoxValid = false;
		return;
	}

	const QRectF now = boxRect();
	if (now.isEmpty())
		return;
	if (!m_labelBoxValid)
	{
		// THE FIRST FRAME THIS LABEL HAS EVER HAD: this node has just become a
		// parent. Until now it was its own name and nothing else, so the name
		// sat on the node's origin - and that origin is now somewhere in the
		// MIDDLE of the frame, with the name lying across whatever the node
		// has come to hold. A parent's name belongs above its frame.
		//
		// Only when nobody has put the name anywhere themselves: an offset
		// that is not the origin was either dragged there or read from a file,
		// and either way it is not ours to overrule.
		m_labelBox = now;
		m_labelBoxValid = true;
		if (m_labelOffset.isNull())
			placeLabelAboveBox();
		return;
	}
	if (now == m_labelBox)
		return;

	const QPointF was = m_labelOffset;   // the label's centre, in our coordinates
	const QPointF next(follow(was.x(), m_labelBox.left(), m_labelBox.right(), now.left(), now.right()),
	                   follow(was.y(), m_labelBox.top(), m_labelBox.bottom(), now.top(), now.bottom()));
	m_labelBox = now;
	if (next == was)
		return;

	// Moving the label cannot move the box: boxRect() is built from
	// contentFrame(), which leaves our own label out. So this settles, and
	// does not chase itself round.
	prepareGeometryChange();
	ancestorsPrepareGeometryChange();
	m_labelOffset = next;
	placeLabel();
	update();
	ancestorsUpdate();
}

void Node::scheduleFrameRefresh()
{
	if (m_frameRefreshQueued)
		return;
	m_frameRefreshQueued = true;
	// queued on us: if we are destroyed first, the call is dropped with our
	// other posted events
	QMetaObject::invokeMethod(this, [this] {
		m_frameRefreshQueued = false;
		refreshFrame();
	}, Qt::QueuedConnection);
}

void Node::ancestorsPrepareGeometryChange()
{
	for (QGraphicsItem* p = parentItem(); p != nullptr; p = p->parentItem())
		if (auto* node = dynamic_cast<Node*>(p))
			node->prepareGeometryChange();
}

void Node::ancestorsUpdate()
{
	for (QGraphicsItem* p = parentItem(); p != nullptr; p = p->parentItem())
	{
		p->update();
		// Their frames are unions of what they hold, so ours changing shape
		// changes theirs, and their labels go with them. Queued and coalesced,
		// so a drag that fires this on every mouse move settles once.
		if (auto* node = dynamic_cast<Node*>(p))
			node->settleLabelSoon();
	}

	// A frame here is the union of what the node holds, so it SHRINKS when a
	// child is deleted or dragged out. update() only repaints the rect an item
	// has NOW, and the deferred refresh runs after the child has already gone,
	// so the pixels of the larger rect it had a moment ago are never painted
	// over - they stand on the canvas as a residue of the old frame. Repaint
	// the scene instead of trying to remember every rect that has been:
	// diagrams here are small, and a view repaints only what it can see.
	if (QGraphicsScene* diagram = scene())
		diagram->update();
}

namespace
{
	// a colour chip: a small square button in that colour
	QToolButton* chip(const QColor& c, const QString& tip, QWidget* parent)
	{
		auto* b = new QToolButton(parent);
		b->setFixedSize(18, 18);
		b->setToolTip(tip);
		b->setCursor(Qt::PointingHandCursor);
		b->setStyleSheet(QString("QToolButton { background: %1; border: 1px solid rgba(0,0,0,90); border-radius: 3px; }"
		                         "QToolButton:hover { border: 2px solid white; }").arg(c.isValid() ? c.name(QColor::HexArgb) : "transparent"));
		return b;
	}

	const char* const kPalette[] = {
		"#ffffff", "#e5e7eb", "#9ca3af", "#4b5563", "#1f2937", "#000000",
		"#fee2e2", "#fecaca", "#ef4444", "#b91c1c", "#fff7ed", "#f97316",
		"#fef9c3", "#facc15", "#dcfce7", "#22c55e", "#15803d", "#ccfbf1",
		"#14b8a6", "#dbeafe", "#3b82f6", "#1d4ed8", "#ede9fe", "#8b5cf6",
	};
}

QMenu* Node::colourMenu(const QString& title, const QColor& current, QMenu* parent, std::function<void(const QColor&)> apply)
{
	auto* menu = parent->addMenu(title);
	auto* grid = new QWidget(menu);
	auto* layout = new QGridLayout(grid);
	layout->setContentsMargins(8, 6, 8, 6);
	layout->setSpacing(4);
	const int cols = 6;
	int i = 0;
	for (const char* hex : kPalette)
	{
		QColor c(hex);
		auto* b = chip(c, c.name(), grid);
		QObject::connect(b, &QToolButton::clicked, menu, [menu, apply, c] { apply(c); menu->close(); });
		layout->addWidget(b, i / cols, i % cols);
		++i;
	}
	auto* action = new QWidgetAction(menu);
	action->setDefaultWidget(grid);
	menu->addAction(action);
	menu->addSeparator();
	menu->addAction("None (transparent)", [apply] { apply(QColor()); });
	menu->addAction("Custom...", [apply, current] {
		const QColor c = QColorDialog::getColor(current.isValid() ? current : Qt::white, nullptr, "Pick a colour", QColorDialog::ShowAlphaChannel);
		if (c.isValid()) apply(c);
	});
	return menu;
}

Category* Node::category() const
{
	Category* C = nullptr;	
	C = dynamic_cast<Category*>(parentItem());

	if (C != nullptr)
	{
		return C;
	}

	if (parentItem() != nullptr)
	{
		Node* node = dynamic_cast<Node*>(parentItem());

		if (node != nullptr)
			return node->category();
	}
	return nullptr;
}

void Node::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
	m_contextPos = event->pos();   // a menu entry may be about this very spot
	popupContextMenu(event->screenPos());
	event->accept();
}

void Node::popupContextMenu(const QPoint& screenPos, const QPointF& itemPos)
{
	m_contextPos = itemPos;
	popupContextMenu(screenPos);
}

void Node::popupContextMenu(const QPoint& screenPos)
{
	QMenu menu;
	menu.setToolTipsVisible(true);   // a QMenu hides tooltips unless asked
	populateContextMenu(menu);
	menu.exec(screenPos);
}


void Node::applyExistsDash(QPen& pen)
{
	if (pen.style() == Qt::NoPen)
		return;   // nothing is being drawn: nothing to dash
	// A dash pattern is measured in PEN WIDTHS, so divide the lengths we want
	// by the width to get the same dash out of a hairline border and out of
	// the thick pen an error or a highlight repaints with. A width of 0 is
	// Qt's cosmetic one-pixel pen, which dashes as though it were 1.
	const qreal width = pen.widthF() > 0.0 ? pen.widthF() : 1.0;
	pen.setDashPattern({ ExistsDashLength / width, ExistsDashGap / width });
	// Flat ends, not round: a round cap adds half the width to each end of
	// every dash, which on a thick line closes the gaps back up.
	pen.setCapStyle(Qt::FlatCap);
}

QString Node::sentenceCase(const QString& text)
{
	return text.isEmpty() ? text : text.left(1).toUpper() + text.mid(1);
}

QString Node::contextTitle() const
{
	return id().isEmpty() ? QStringLiteral("node") : id();
}

void Node::addObjectAction(QMenu& menu, Category* home)
{
	if (home == nullptr)
		return;
	auto* diagram = diagramOf(this);
	if (diagram == nullptr)
		return;
	// where the menu was opened, in the scene - so the object lands under the
	// cursor rather than in the middle of whatever it is going into
	const QPointF at = mapToScene(contextPos());
	QAction* add = menu.addAction(QString("%1  Add object").arg(Emoji::add()));
	add->setToolTip(QString("Place a new object in %1, here.").arg(home->id()));
	QObject::connect(add, &QAction::triggered, diagram, [diagram, home, at] {
		// Queued: the menu is still closing, and this adds to the very scene
		// the menu belongs to.
		QMetaObject::invokeMethod(diagram, [diagram, home, at] {
			Object* placed = home->createCanvasObject(at);
			if (placed != nullptr)
				diagram->recordCreation(
					QString("Placed %1 in %2").arg(placed->id(), home->id()), { placed });
		}, Qt::QueuedConnection);
	});
	menu.addSeparator();
}

void Node::populateContextMenu(QMenu& menu)
{
	// 1. what this is
	menu.addAction(contextTitle())->setEnabled(false);
	menu.addSeparator();

	// 2. what it can do: each kind of node fills this in with submenus
	populateActions(menu);

	// 3. how it is drawn
	QMenu* look = menu.addMenu(Emoji::appearance() + "  Appearance");
	colourMenu("Fill", m_fill.style() == Qt::NoBrush ? QColor() : m_fill.color(), look, [this](const QColor& c) {
		const QBrush before = m_fill;
		const QPen borderBefore = m_border;
		setFill(c.isValid() ? QBrush(c) : QBrush(Qt::NoBrush));
		recordStyleChange(before, borderBefore);
	});
	colourMenu("Border", m_border.style() == Qt::NoPen ? QColor() : m_border.color(), look, [this](const QColor& c) {
		const QBrush fillBefore = m_fill;
		const QPen before = m_border;
		if (!c.isValid())
		{
			setBorder(Qt::NoPen);
		}
		else
		{
			QPen p = m_border.style() == Qt::NoPen ? QPen(c, 1.5) : m_border;
			p.setColor(c);
			setBorder(p);
		}
		recordStyleChange(fillBefore, before);
	});

	// 4. what it asserts
	QAction* exists = menu.addAction(Emoji::existsSuch() + "  Exists such");
	exists->setCheckable(true);
	exists->setChecked(m_existsSuch);
	exists->setToolTip("Draw this dotted, and read it as the part that is claimed to EXIST: for all the "
	                   "solid objects and arrows, there is such a one making the diagram commute.");
	Node* self = this;
	QObject::connect(exists, &QAction::toggled, &menu, [self](bool on) { self->setExistsSuchRecorded(on); });

	QAction* cross = menu.addAction(Emoji::deleteMark() + "  Delete on apply");
	cross->setCheckable(true);
	cross->setChecked(m_deleteMark);
	cross->setToolTip("Cross this out in red. Read as a rule, the diagram still has to FIND this - but "
	                  "where the rule is applied, this is what gets taken out.");
	QObject::connect(cross, &QAction::toggled, &menu, [self](bool on) { self->setDeleteMarkRecorded(on); });

	// 5. and getting rid of it - last, and on its own, because it is the one
	// entry here that cannot be half-done. Queued: the menu is still closing
	// when this runs, and this node is what the menu belongs to.
	auto* diagram = dynamic_cast<DiagramScene*>(scene());
	if (diagram != nullptr && diagram->ambientCategory() != this)
	{
		menu.addSeparator();
		QAction* remove = menu.addAction(QString("%1  Delete %2")
			.arg(Emoji::remove(), id().isEmpty() ? QStringLiteral("this node") : id()));
		remove->setShortcut(QKeySequence::Delete);
		QObject::connect(remove, &QAction::triggered, diagram, [diagram, self] {
			QMetaObject::invokeMethod(diagram, [diagram, self] { diagram->deleteNode(self); }, Qt::QueuedConnection);
		});
	}
}

Category* Node::surroundingCategory() const
{
	for (QGraphicsItem* p = parentItem(); p != nullptr; p = p->parentItem())
		if (auto* category = dynamic_cast<Category*>(p))
			return category;
	return nullptr;
}

void Node::recordStyleChange(const QBrush& fillBefore, const QPen& borderBefore)
{
	if (m_fill == fillBefore && m_border == borderBefore)
		return;
	m_styleChosen = true;   // asked for by hand: draw it whatever this node holds
	auto* diagram = dynamic_cast<DiagramScene*>(scene());
	if (diagram == nullptr)
		return;
	diagram->history()->record(new StyleChanged(
		QString("Recoloured %1").arg(id().isEmpty() ? QStringLiteral("a node") : id()),
		this, fillBefore, borderBefore, m_fill, m_border));
}

DiagramScene* Node::diagramOf(const Node* node)
{
	return node == nullptr ? nullptr : dynamic_cast<DiagramScene*>(node->scene());
}

void Node::setExistsSuchRecorded(bool existsSuch)
{
	if (m_existsSuch == existsSuch)
		return;
	const bool before = m_existsSuch;
	setExistsSuch(existsSuch);
	if (auto* diagram = diagramOf(this))
		diagram->noteExistsSuch(this, before, existsSuch);
}

void Node::setDeleteMarkRecorded(bool marked)
{
	if (m_deleteMark == marked)
		return;
	const bool before = m_deleteMark;
	setDeleteMark(marked);
	if (auto* diagram = diagramOf(this))
		diagram->noteDeleteMark(this, before, marked);
}

// ---------------------------------------------------------------- editing the label

void Node::editLabel()
{
	if (m_idText != nullptr)
		m_idText->beginEdit();
}

void Node::labelBeingEdited(const QString& text)
{
	// as it is typed: recentre, reframe, and tell everything that watches this
	// label - a product of it, a functor's image of it - so the diagram keeps
	// up with the keyboard
	placeLabel();
	refreshFrame();
	emit idChanged(this, text);
}

void Node::finishLabelEdit(const QString& before, const QString& typed)
{
	// What was typed is what is kept: 1_X stays 1_X. The subscript is put on
	// by the label when it draws itself (see Notation), so the markers survive
	// saving, matching, and opening the editor again.
	const QString after = typed;

	placeLabel();
	refreshFrame();
	emit idChanged(this, after);   // after a cancel this puts the watchers back
	if (before == after)
		return;

	// what was typed may name other nodes: Hom(X,B) follows X and B from here on
	bindLabelReferences();
	if (auto* diagram = diagramOf(this))
		diagram->history()->record(new Renamed(
			QString("Renamed %1 to %2").arg(before.isEmpty() ? QStringLiteral("a node") : before, after),
			this, before, after));
}

// ---------------------------------------------------------------- labels made of labels

QList<Node*> Node::labelSources() const
{
	QList<Node*> sources;
	for (const QPointer<Node>& source : m_labelSources)
		if (!source.isNull())
			sources << source.data();
	return sources;
}

void Node::setDerivedLabel(const QString& pattern, const QList<Node*>& sources)
{
	clearDerivedLabel();
	m_labelPattern = pattern;
	for (Node* source : sources)
	{
		if (source == nullptr)
			continue;
		m_labelSources.append(QPointer<Node>(source));
		connect(source, &Node::idChanged, this, &Node::refreshDerivedLabel);
		connect(source, &Node::deleted, this, &Node::refreshDerivedLabel);
	}
	refreshDerivedLabel();
}

void Node::clearDerivedLabel()
{
	for (const QPointer<Node>& source : m_labelSources)
		if (!source.isNull())
			disconnect(source.data(), nullptr, this, nullptr);
	m_labelSources.clear();
	m_labelPattern.clear();
}

void Node::refreshDerivedLabel()
{
	if (m_labelPattern.isEmpty())
		return;
	QString text = m_labelPattern;
	for (int i = 0; i < m_labelSources.size(); ++i)
	{
		const QPointer<Node>& source = m_labelSources.at(i);
		text.replace("%" + QString::number(i + 1),
		             source.isNull() ? QStringLiteral("?") : source->id());
	}
	m_settingDerivedLabel = true;   // setting it must not throw the formula away
	setId(text);
	m_settingDerivedLabel = false;
}

void Node::pushSiblings(const QPointF& delta)
{
	QGraphicsItem* parent = parentItem();
	if (!s_collisionEnabled || parent == nullptr)
		return;
	// a hand on the mouse, or a shove already under way that this one is part
	// of; never a position set by the program itself
	if (!s_userDragging && s_pushDepth == 0)
		return;

	const qreal travelled = QLineF(QPointF(0, 0), delta).length();
	if (travelled < 1e-6)
		return;
	const QPointF direction = delta / travelled;   // the way this node is going

	// A shove can start another, and two nodes can shove each other: count the
	// depth and remember who has already been moved this round.
	if (s_pushDepth == 0)
		s_pushed.clear();
	if (s_pushDepth > 8)
		return;
	++s_pushDepth;
	s_pushed.insert(this);

	auto* diagram = diagramOf(this);
	// boxes shove boxes: a label dragged clear of its frame is not a wall
	const QRectF mine = mapRectToParent(boxRect());

	for (QGraphicsItem* item : parent->childItems())
	{
		auto* other = dynamic_cast<Node*>(item);
		if (other == nullptr || other == this)
			continue;
		if (dynamic_cast<Arrow*>(other) != nullptr)
			continue;   // an arrow is drawn from its ends; it is not in the way
		// The canvas itself does not get shoved. Objects are no longer marked
		// movable - the scene carries them by hand - so being the ambient
		// category is the thing to test, not the flag.
		if (diagram != nullptr && diagram->ambientCategory() == other)
			continue;
		if (s_pushed.contains(other))
			continue;
		if (s_userDragging && other->isSelected() && isSelected())
			continue;   // both are in the same drag: Qt is moving them together
		if (!mine.intersects(other->mapRectToParent(other->boxRect())))
			continue;

		// Only what is in FRONT of the push: a neighbour we are moving away
		// from is not being pushed into. Level with us counts as in front
		// (dot == 0), and so does sitting exactly on us - two nodes stacked
		// centre on centre have nowhere else to be sorted out.
		const QPointF away = other->mapRectToParent(other->boxRect()).center() - mine.center();
		if (QPointF::dotProduct(away, direction) < 0)
			continue;

		// Where it started, so undo puts it back with the node that shoved it.
		if (diagram != nullptr)
			diagram->notePushed(other, other->pos());

		// Far enough to be clear, and epsilon further so they are not left
		// merely touching. Repeated because the grid may round the landing
		// back into us: each pass is strictly further along the push.
		for (int attempt = 0; attempt < 8; ++attempt)
		{
			const QRectF theirs = other->mapRectToParent(other->boxRect());
			if (!mine.intersects(theirs))
				break;
			qreal step = separation(mine, theirs, direction) + s_pushEpsilon;
			// The landing is put on the grid, so a shove shorter than half a
			// unit would be rounded straight back onto where it started. Ask
			// for whole grid units when the grid is on.
			if (s_snapEnabled && s_snapUnit > 0)
				step = qCeil(step / s_snapUnit) * s_snapUnit;
			const QPointF before = other->pos();
			other->setPos(other->pos() + direction * step);
			if (other->pos() == before)
				break;   // it will not move (nothing to gain by asking again)
		}
	}

	--s_pushDepth;
}

namespace
{
	// what counts as part of a name, for finding whole ones inside a formula
	bool isNameChar(QChar c)
	{
		return c.isLetterOrNumber() || c == QLatin1Char('_')
		    || c == QChar(0x2032)                      // prime
		    || (c.unicode() >= 0x2080 && c.unicode() <= 0x2089);   // subscript digits
	}
}

void Node::bindLabelReferences()
{
	const QString text = id();
	auto* diagram = diagramOf(this);
	if (text.isEmpty() || diagram == nullptr)
		return;

	// longest names first, so ABC is not read as A followed by BC
	QList<Node*> candidates = diagram->labelledNodes(this);
	std::sort(candidates.begin(), candidates.end(), [](const Node* a, const Node* b) {
		return a->id().size() > b->id().size();
	});

	QString pattern;
	QList<Node*> sources;
	int at = 0;
	while (at < text.size())
	{
		Node* found = nullptr;
		for (Node* candidate : candidates)
		{
			const QString name = candidate->id();
			if (name.isEmpty() || !QStringView(text).mid(at).startsWith(name))
				continue;
			// a whole name, not a piece of a longer word
			const bool leftClear = at == 0 || !isNameChar(text.at(at - 1));
			const bool rightClear = at + name.size() >= text.size() || !isNameChar(text.at(at + name.size()));
			if (leftClear && rightClear)
			{
				found = candidate;
				break;
			}
		}
		if (found == nullptr)
		{
			pattern += text.at(at);
			++at;
			continue;
		}
		int index = sources.indexOf(found);
		if (index < 0)
		{
			sources << found;
			index = sources.size() - 1;
		}
		pattern += "%" + QString::number(index + 1);
		at += found->id().size();
	}

	if (sources.isEmpty())
	{
		clearDerivedLabel();   // it names nothing: just a label
		return;
	}
	setDerivedLabel(pattern, sources);
}
