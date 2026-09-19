#include "art/Arrow.h"

#include <QMenu>
#include <QActionGroup>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include "core/props/ArrowProp.h"
#include "core/props/ArrowProps.h"
#include "art/DiagramScene.h"
#include "art/Category.h"
#include "core/AppSettings.h"
#include "core/Emoji.h"
#include "core/history/SceneHistory.h"
#include "core/history/Mementos.h"
#include <QPainterPathStroker>
#include <QStyleOptionGraphicsItem>
#include <QtMath>
#include "art/GraphicsHelpers.h"
#include <QDebug>

namespace
{
	// where a ray from the centre of `rect` in direction `dir` leaves it
	QPointF exitPoint(const QRectF& rect, const QPointF& dir)
	{
		const QPointF c = rect.center();
		qreal t = 1e9;
		if (!qFuzzyIsNull(dir.x()))
			t = qMin(t, ((dir.x() > 0 ? rect.right() : rect.left()) - c.x()) / dir.x());
		if (!qFuzzyIsNull(dir.y()))
			t = qMin(t, ((dir.y() > 0 ? rect.bottom() : rect.top()) - c.y()) / dir.y());
		return c + dir * (t < 1e9 ? t : 0);
	}
}

Arrow::Arrow(const QString& id, Node* domain, Node* codomain, QGraphicsItem *parent)
	: Node(id, parent)
{
	setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemSendsGeometryChanges);   // never dragged: the ends place it
	setDefaultLook(QBrush(Qt::NoBrush), QPen(QColor(30, 144, 255), 1.5));

	// AND WHAT WAS ASKED FOR, which wins over the look above.
	//
	// "Set default" on the Properties page stores the colours the next one
	// placed should start in. Written through setFill/setBorder, so it DOES
	// count as chosen: somebody picked these, and a node picked out that way
	// shows its frame whether or not it holds anything, exactly as one
	// coloured by hand does.
	const QColor wantedFill = AppSettings::instance().defaultFill(true);
	const QColor wantedBorder = AppSettings::instance().defaultBorder(true);
	if (wantedFill.isValid())
		setFill(QBrush(wantedFill));
	if (wantedBorder.isValid())
		setBorder(QPen(wantedBorder, AppSettings::instance().arrowLineWidth()));
	// an arrow's label gets in the way of whatever the line crosses, so it can
	// be picked up and put somewhere clearer
	if (NodeLabel* text = labelItem())
	{
		text->setFlag(QGraphicsItem::ItemIsMovable, true);
		text->setCursor(Qt::SizeAllCursor);
	}
	setAcceptHoverEvents(true);   // the bends show themselves when pointed at
	setDomain(domain);
	setCodomain(codomain);
	refreshGeometry();
}

void Arrow::setDomain(Node* domain)
{
	if (domain != m_domain)
	{
		if (m_domain != nullptr)
			disconnectFromObject(m_domain);
		m_domain = domain;
		if (domain != nullptr)
			connectToObject(domain);
		refreshGeometry();
		emit domainChanged(domain);
	}
}

void Arrow::setCodomain(Node* codomain)
{
	if (codomain != m_codomain)
	{
		if (m_codomain != nullptr)
			disconnectFromObject(m_codomain);
		m_codomain = codomain;
		if (codomain != nullptr)
			connectToObject(codomain);
		refreshGeometry();
		emit codomainChanged(codomain);
	}
}

Arrow::~Arrow()
{
	// Log and remove from scene as a defensive measure against painting while
	// the vtable of the derived object is being torn down.
	safeRemoveAndLog(this, "Arrow");
	if (m_domain != nullptr)
		disconnectFromObject(m_domain);
	if (m_codomain != nullptr)
		disconnectFromObject(m_codomain);
}

void Arrow::onObjectDeleted(Node* object)
{
	if (m_domain == object)
		m_domain = nullptr;
	if (m_codomain == object)
		m_codomain = nullptr;

	// Out of sight AT ONCE, and only then deleteLater. Between this and the
	// event loop coming back round, the scene may repaint or hit-test any
	// number of times, and every one of those walks an arrow with an end
	// that no longer exists. Hiding it first is what keeps the half-dead
	// arrow out of all of that - and it is also why a deleted arrow no
	// longer flickers in and out of the canvas until its turn comes.
	setVisible(false);
	setAcceptedMouseButtons(Qt::NoButton);
	deleteLater();
}

void Arrow::connectToObject(Node * object)
{
	connect(object, &Node::deleted, this, &Arrow::onObjectDeleted);
	connect(object, &Node::moved, this, &Arrow::onObjectMoved);
}

void Arrow::disconnectFromObject(Node* object)
{
	disconnect(object, &Node::deleted, this, &Arrow::onObjectDeleted);
	disconnect(object, &Node::moved, this, &Arrow::onObjectMoved);
}

namespace
{
	// THE PATH WITH `cut` TAKEN OFF ITS FAR END, measured along the curve.
	//
	// Qt can tell you how long a path is and where a given length along it
	// falls, but it cannot hand you back a piece of one, so this walks the
	// curve and rebuilds it as far as the cut. The far END is what has to land
	// exactly - it is being fitted against the arrowhead - and it does:
	// percentAtLength puts the last point precisely where it was asked for.
	// The samples between are only there to keep a bend looking like a bend,
	// which is why a few per pixel of length is plenty and a straight arrow
	// (nearly all of them) comes out straight whatever the count.
	QPainterPath trimmedAtEnd(const QPainterPath& path, qreal cut)
	{
		const qreal total = path.length();
		if (cut <= 0.0 || total <= 1e-6)
			return path;
		if (cut >= total)
			return QPainterPath();   // the head has eaten the whole line

		const qreal stop = path.percentAtLength(total - cut);
		const int steps = qBound(2, int((total - cut) / 3.0), 240);
		QPainterPath out;
		out.moveTo(path.pointAtPercent(0.0));
		for (int i = 1; i <= steps; ++i)
			out.lineTo(path.pointAtPercent(stop * qreal(i) / steps));
		return out;
	}

	qreal norm(const QPointF& p)
	{
		return qSqrt(p.x() * p.x() + p.y() * p.y());
	}
}

bool Arrow::segment(QPointF& from, QPointF& to) const
{
	const QList<QPointF> pts = throughPoints();
	if (pts.size() < 2)
		return false;
	from = pts.first();
	to = pts.last();
	return true;
}

namespace
{
	// An arrow may end on an arrow, which may itself end on an arrow. Asking
	// each one for its curve walks that chain, and the user can close it into
	// a ring (u : p -> v beside v : q -> u), which would recur until the stack
	// gives out. Only ever descend a few links.
	int g_endFrameDepth = 0;
	const int kMaxEndFrameDepth = 4;
}

QRectF Arrow::endFrame(Node* end) const
{
	// An arrow that is the end of another arrow (a 2-cell, a natural
	// transformation) has a bounding rect covering the whole span of its
	// curve, which would put the join out in the open. Aim at the middle of
	// the line instead.
	if (auto* arrow = dynamic_cast<Arrow*>(end))
	{
		if (g_endFrameDepth < kMaxEndFrameDepth)
		{
			++g_endFrameDepth;
			const QPainterPath path = arrow->curve();
			--g_endFrameDepth;
			if (!path.isEmpty())
			{
				const QPointF middle = mapFromItem(arrow, path.pointAtPercent(0.5));
				return QRectF(middle - QPointF(7, 7), QSizeF(14, 14));
			}
		}
		// too deep, or it has no line yet: aim at where it sits, WITHOUT
		// asking for a bounding rect (that would walk the same chain again)
		return mapFromItem(arrow, QRectF(-7, -7, 14, 14)).boundingRect();
	}
	// its box, not its bounding rect: an arrow stops at the frame it points
	// at, not at a name sitting above that frame
	return mapFromItem(end, end->boxRect()).boundingRect();
}


QPointF Arrow::onGrid(const QPointF& point, bool snapX, bool snapY) const
{
	const qreal unit = Node::snapUnit();
	if (!Node::snapEnabled() || unit <= 0 || (!snapX && !snapY))
		return point;
	// the grid is measured in SCENE units, so go out there to round it
	QPointF scene = mapToScene(point);
	if (snapX)
		scene.setX(qRound(scene.x() / unit) * unit);
	if (snapY)
		scene.setY(qRound(scene.y() / unit) * unit);
	return mapFromScene(scene);
}

QPointF Arrow::attachTo(const QRectF& frame, const QPointF& from) const
{
	// the nearest point of the frame to where the line is coming from...
	QPointF at(qBound(frame.left(), from.x(), frame.right()),
	           qBound(frame.top(), from.y(), frame.bottom()));

	// ...and if that is inside, out to the nearest edge
	if (frame.contains(at))
	{
		const qreal dl = at.x() - frame.left();
		const qreal dr = frame.right() - at.x();
		const qreal dt = at.y() - frame.top();
		const qreal db = frame.bottom() - at.y();
		const qreal nearest = qMin(qMin(dl, dr), qMin(dt, db));
		if (nearest == dl)      at.setX(frame.left());
		else if (nearest == dr) at.setX(frame.right());
		else if (nearest == dt) at.setY(frame.top());
		else                    at.setY(frame.bottom());
	}

	// on the grid, but only along the edge it sits on - snapping the other
	// coordinate would lift the join off the frame
	const bool onVerticalEdge = qFuzzyCompare(at.x(), frame.left()) || qFuzzyCompare(at.x(), frame.right());
	QPointF snapped = onGrid(at, !onVerticalEdge, onVerticalEdge);
	snapped.setX(qBound(frame.left(), snapped.x(), frame.right()));
	snapped.setY(qBound(frame.top(), snapped.y(), frame.bottom()));
	return snapped;
}

namespace
{
	// The nearest pair of points on two frames, one on each. Where the frames
	// overlap on an axis, both points share a coordinate in that overlap -
	// which is what makes two boxes side by side join up with a straight
	// horizontal line rather than a slope between their centres.
	void nearestPair(const QRectF& a, const QRectF& b, QPointF& from, QPointF& to,
	                 bool& sharedX, bool& sharedY)
	{
		sharedX = sharedY = false;
		if (a.right() < b.left())      { from.setX(a.right()); to.setX(b.left()); }
		else if (b.right() < a.left()) { from.setX(a.left());  to.setX(b.right()); }
		else
		{
			const qreal lo = qMax(a.left(), b.left());
			const qreal hi = qMin(a.right(), b.right());
			from.setX((lo + hi) / 2);
			to.setX(from.x());
			sharedX = true;
		}

		if (a.bottom() < b.top())      { from.setY(a.bottom()); to.setY(b.top()); }
		else if (b.bottom() < a.top()) { from.setY(a.top());    to.setY(b.bottom()); }
		else
		{
			const qreal lo = qMax(a.top(), b.top());
			const qreal hi = qMin(a.bottom(), b.bottom());
			from.setY((lo + hi) / 2);
			to.setY(from.y());
			sharedY = true;
		}
	}
}

void Arrow::setInert()
{
	setAcceptedMouseButtons(Qt::NoButton);
	setAcceptHoverEvents(false);
	setFlag(QGraphicsItem::ItemIsSelectable, false);
	if (NodeLabel* text = labelItem())
	{
		text->setAcceptedMouseButtons(Qt::NoButton);
		text->setFlag(QGraphicsItem::ItemIsMovable, false);
	}
}

void Arrow::setLooseEnd(const QPointF& scenePos)
{
	if (m_hasLooseEnd && m_looseEnd == scenePos)
		return;
	m_hasLooseEnd = true;
	m_looseEnd = scenePos;
	refreshGeometry();
}

void Arrow::clearLooseEnd()
{
	if (!m_hasLooseEnd)
		return;
	m_hasLooseEnd = false;
	refreshGeometry();
}

QList<QPointF> Arrow::throughPoints() const
{
	QList<QPointF> pts;
	if (m_domain == nullptr)
		return pts;
	// No codomain and nowhere to run to: there is no line yet.
	if (m_codomain == nullptr && !m_hasLooseEnd)
		return pts;
	const QRectF rd = endFrame(m_domain);
	// A point at the cursor stands in for the frame this will eventually stop
	// at, so the very same joining code draws the half-placed arrow - it comes
	// off its domain's edge exactly as the finished one will.
	const QRectF rc = m_codomain != nullptr
		? endFrame(m_codomain)
		: QRectF(mapFromScene(m_looseEnd), QSizeF(0.01, 0.01));

	// A BEND INSIDE ONE OF THE ENDS IS NOT A SHAPE, IT IS A MISTAKE.
	//
	// A point dropped within the frame of the thing the arrow runs into
	// cannot be seen - it is under the object - and it wrecks the line: the
	// end attaches at the spot on the frame nearest THAT point, so the line
	// carries on past the object and the head is drawn inside it, aimed back
	// out. (That is what a slip near the end used to leave behind, before a
	// press there stopped counting as a bend.)
	//
	// Such points are left in the arrow - they are the person's, and undo may
	// want them - but the line is not drawn through them.
	QList<QPointF> drawnBends;
	drawnBends.reserve(m_bends.size());
	for (const QPointF& bend : m_bends)
		if (!rd.contains(bend) && !rc.contains(bend))
			drawnBends << bend;

	QPointF start, end;
	if (drawnBends.isEmpty())
	{
		// Nothing pulling it about: join the two frames at their nearest
		// points. Two boxes beside each other meet on the line between their
		// facing edges, so the arrow comes out horizontal rather than sloping
		// from one centre to the other.
		bool sharedX = false, sharedY = false;
		nearestPair(rd, rc, start, end, sharedX, sharedY);

		if (sharedX && sharedY)
		{
			// the frames overlap: there is no gap to draw in, so fall back to
			// the old centre-to-centre join
			QPointF out = rc.center() - rd.center();
			const qreal length = norm(out);
			if (length < 1e-6)
				return pts;
			out /= length;
			start = exitPoint(rd, out) + out * 3;
			end = exitPoint(rc, -out) - out * 3;
		}
		else
		{
			// the join runs along one axis: put the shared coordinate on the
			// grid so the line is level, and keep it on both frames
			if (sharedY)
			{
				const qreal lo = qMax(rd.top(), rc.top());
				const qreal hi = qMin(rd.bottom(), rc.bottom());
				const qreal y = qBound(lo, onGrid(start, false, true).y(), hi);
				start.setY(y);
				end.setY(y);
			}
			if (sharedX)
			{
				const qreal lo = qMax(rd.left(), rc.left());
				const qreal hi = qMin(rd.right(), rc.right());
				const qreal x = qBound(lo, onGrid(start, true, false).x(), hi);
				start.setX(x);
				end.setX(x);
			}
			// a hair of air at each end
			QPointF along = end - start;
			const qreal length = norm(along);
			if (length > 1e-6)
			{
				along /= length;
				start += along * 3;
				end -= along * 3;
			}
		}
	}
	else
	{
		// pulled about: each end joins at the point nearest the bend it is
		// heading for
		start = attachTo(rd, drawnBends.first());
		end = attachTo(rc, drawnBends.last());

		QPointF out = drawnBends.first() - start;
		qreal length = norm(out);
		if (length > 1e-6)
			start += (out / length) * 3;
		QPointF in = end - drawnBends.last();
		length = norm(in);
		if (length > 1e-6)
			end -= (in / length) * 3;
	}

	// A VEE TAIL IS NOT SOMETHING THE LINE RUNS THROUGH.
	//
	// The vee is drawn backwards from its point, with its open ends against
	// the domain, and the line proper starts AT that point. Left alone the
	// line ran the whole way and came out through the middle of the vee. So
	// the start is moved along by a head's length and the vee fills the gap.
	//
	// Not for a hook - an inclusion's line really does run into its hook - and
	// not on an arrow too short to give up the room.
	if (m_style == Style::Mono || (isMonic() && !isInclusion()))
	{
		const QPointF toward = drawnBends.isEmpty() ? end : drawnBends.first();
		QPointF along = toward - start;
		const qreal length = norm(along);
		const qreal room = AppSettings::instance().arrowHeadLength();
		if (length > room * 1.5)
			start += (along / length) * room;
	}

	pts << start;
	pts << drawnBends;
	pts << end;
	return pts;
}
QPainterPath Arrow::curve() const
{
	QPainterPath path;
	const QList<QPointF> pts = throughPoints();
	if (pts.size() < 2)
		return path;
	path.moveTo(pts.first());
	if (pts.size() == 2)
	{
		path.lineTo(pts.last());
		return path;
	}
	// Catmull-Rom, written as the cubic Beziers QPainterPath draws: the curve
	// runs THROUGH every point, with the ends held by doubling them up
	for (int i = 0; i + 1 < pts.size(); ++i)
	{
		const QPointF p0 = pts.at(qMax(0, i - 1));
		const QPointF p1 = pts.at(i);
		const QPointF p2 = pts.at(i + 1);
		const QPointF p3 = pts.at(qMin(int(pts.size()) - 1, i + 2));
		path.cubicTo(p1 + (p2 - p0) / 6.0, p2 - (p3 - p1) / 6.0, p2);
	}
	return path;
}

void Arrow::setBends(const QList<QPointF>& bends)
{
	if (m_bends == bends)
		return;
	prepareGeometryChange();
	ancestorsPrepareGeometryChange();
	m_bends = bends;
	refreshGeometry();
	// an arrow may be the end of another arrow: its shape changed, so anything
	// hanging off it has to be redrawn
	emit moved(this, QPointF());
	emit bendsChanged(this);
}

void Arrow::recordBends(const QString& what, const QList<QPointF>& before)
{
	if (before == m_bends)
		return;
	if (auto* diagram = diagramOf(this))
		diagram->history()->record(new ArrowBent(what, this, before, m_bends));
}

int Arrow::bendIndexFor(const QPointF& pos) const
{
	// the leg of the run this point belongs on
	const QList<QPointF> pts = throughPoints();
	int best = 0;
	qreal bestDistance = -1;
	for (int i = 0; i + 1 < pts.size(); ++i)
	{
		const QLineF leg(pts.at(i), pts.at(i + 1));
		const qreal length = leg.length();
		qreal distance = 0;
		if (length < 1e-6)
		{
			distance = norm(pos - leg.p1());
		}
		else
		{
			const QPointF d = leg.p2() - leg.p1();
			qreal t = QPointF::dotProduct(pos - leg.p1(), d) / (length * length);
			t = qBound(qreal(0), t, qreal(1));
			distance = norm(pos - (leg.p1() + d * t));
		}
		if (bestDistance < 0 || distance < bestDistance)
		{
			bestDistance = distance;
			best = i;
		}
	}
	return best;   // a bend on leg i sits at index i among the bends
}

void Arrow::addBend(const QPointF& at)
{
	QList<QPointF> bends = m_bends;
	bends.insert(qBound(0, bendIndexFor(at), int(bends.size())), at);
	setBends(bends);
}

void Arrow::removeBend(int index)
{
	if (index < 0 || index >= m_bends.size())
		return;
	QList<QPointF> bends = m_bends;
	bends.removeAt(index);
	setBends(bends);
}

void Arrow::straighten()
{
	setBends(QList<QPointF>());
}

void Arrow::straightenRecorded()
{
	if (m_bends.isEmpty())
		return;
	const QList<QPointF> before = m_bends;
	straighten();
	recordBends(QString("Straightened %1").arg(id()), before);
}

int Arrow::bendAt(const QPointF& pos, qreal radius) const
{
	for (int i = 0; i < m_bends.size(); ++i)
		if (norm(pos - m_bends.at(i)) <= radius)
			return i;
	return -1;
}

QPointF Arrow::labelAnchor() const
{
	if (label() == nullptr)
		return QPointF();

	QPointF middle;
	QPointF normal(0, -1);

	const QPainterPath path = curve();
	if (path.isEmpty())
	{
		// No line to hang it beside yet - an end not placed, or the two frames
		// sitting on top of each other. Anywhere is better than this arrow's
		// origin, which is the CATEGORY'S corner, where its own label is.
		if (m_domain == nullptr || m_codomain == nullptr)
			return label()->pos();   // leave it where it is
		middle = (mapFromItem(m_domain, m_domain->boxRect().center())
		        + mapFromItem(m_codomain, m_codomain->boxRect().center())) / 2;
	}
	else
	{
		middle = path.pointAtPercent(0.5);
		QPointF dir = path.pointAtPercent(0.55) - path.pointAtPercent(0.45);
		const qreal length = norm(dir);
		dir = length > 1e-6 ? dir / length : QPointF(1, 0);
		normal = QPointF(-dir.y(), dir.x());
		if (normal.y() > 0)
			normal = -normal;   // above the line, to begin with
	}

	// WHICHEVER SIDE IS FREE.
	//
	// Above the line is the right answer nearly always, and the wrong one
	// exactly where it matters: a composite drawn in by a rule runs diagonally
	// across the square it completes, and ABOVE that diagonal is where the two
	// arrows it is the composite OF already are. The name landed on top of
	// them - g o f written across f and N - which is the one place it must not
	// be.
	//
	// So both sides are tried, and a side that puts the name on top of
	// something loses to one that does not. Only the side is chosen here: a
	// label that has been dragged keeps the side it was dragged to, because
	// its offset is measured from this point and flipping it would move the
	// label out from under the hand that placed it.
	if (!m_labelPlacedByHand && !path.isEmpty())
	{
		const QRectF text = label()->boundingRect();
		auto lands = [&](const QPointF& side) {
			const QPointF at = middle + side * 12;
			return QRectF(at.x() - text.width() / 2, at.y() - text.height() / 2,
			              text.width(), text.height());
		};
		auto blocked = [&](const QRectF& where) {
			QGraphicsItem* parent = parentItem();
			if (parent == nullptr)
				return false;
			for (QGraphicsItem* sibling : parent->childItems())
			{
				auto* node = dynamic_cast<Node*>(sibling);
				if (node == nullptr || node == this || !node->isVisible())
					continue;
				// what it IS - and for an ARROW that is its LINE, not the
				// rectangle around it: a diagonal arrow's bounding box covers
				// the whole square it crosses, which would call every side
				// blocked and settle nothing
				if (auto* line = dynamic_cast<Arrow*>(node); line != nullptr)
				{
					if (mapFromItem(line, line->curve()).intersects(where))
						return true;
				}
				else if (where.intersects(mapFromItem(node, node->boxRect()).boundingRect()))
				{
					return true;
				}
				// and what it is CALLED, which is just as much in the way
				const QRectF name = node->labelRect();
				if (!name.isEmpty() && where.intersects(mapFromItem(node, name).boundingRect()))
					return true;
			}
			return false;
		};
		if (blocked(lands(normal)) && !blocked(lands(-normal)))
			normal = -normal;
	}

	const QRectF text = label()->boundingRect();
	const QPointF at = middle + normal * 12;
	return QPointF(at.x() - text.width() / 2, at.y() - text.height() / 2);
}

bool Arrow::labelFrame(QPointF& along, QPointF& across, qreal& length) const
{
	const QList<QPointF> pts = throughPoints();
	if (pts.size() < 2)
		return false;
	along = pts.last() - pts.first();
	length = norm(along);
	if (length < 1e-6)
		return false;   // the two ends on top of each other: nothing to measure
	along /= length;
	across = QPointF(-along.y(), along.x());
	return true;
}

void Arrow::rememberLabelPlacement()
{
	QPointF along, across;
	qreal length = 0;
	if (!labelFrame(along, across, length))
		return;   // nothing to measure against: keep what was remembered
	m_labelAlong = QPointF::dotProduct(m_labelOffset, along) / length;
	m_labelAcross = QPointF::dotProduct(m_labelOffset, across) / length;
	m_labelPlacedByHand = !m_labelOffset.isNull();
}

void Arrow::placeLabel()
{
	if (label() == nullptr)
		return;
	// The line may have moved, turned or changed length since the label was
	// put where it is. It was remembered as a fraction of that line, so it is
	// worked out again here and stays where it was PUT rather than where the
	// pixels used to be.
	if (m_labelPlacedByHand)
	{
		QPointF along, across;
		qreal length = 0;
		if (labelFrame(along, across, length))
			m_labelOffset = (along * m_labelAlong + across * m_labelAcross) * length;
	}
	label()->setPos(labelAnchor() + m_labelOffset);
}

void Arrow::setLabelOffset(const QPointF& offset)
{
	if (m_labelOffset == offset)
		return;
	const QPointF was = m_labelOffset;
	m_labelOffset = offset;
	rememberLabelPlacement();
	refreshGeometry();
	// said out loud, so a mapping downstream of this one carries it on (see
	// Node::labelOffsetChanged)
	emit labelOffsetChanged(this, m_labelOffset - was);
}

void Arrow::labelMoved(const QPointF& pos)
{
	// what the user dragged is remembered as a displacement, so the label
	// travels with the line rather than staying where the screen was
	const QPointF was = m_labelOffset;
	m_labelOffset = pos - labelAnchor();
	rememberLabelPlacement();   // dragged HERE, against the line as it stands
	prepareGeometryChange();
	ancestorsPrepareGeometryChange();
	update();
	ancestorsUpdate();
	if (m_labelOffset != was)
		emit labelOffsetChanged(this, m_labelOffset - was);
}

void Arrow::labelDragFinished(const QPointF& fromPos)
{
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()))
		diagram->history()->record(new LabelMoved(
			QString("Moved the label of %1").arg(id().isEmpty() ? QStringLiteral("an arrow") : id()),
			this, fromPos - labelAnchor(), m_labelOffset));
}

void Arrow::refreshGeometry()
{
	prepareGeometryChange();
	ancestorsPrepareGeometryChange();
	if (label() != nullptr)
	{
		// beside the middle of the curve, plus wherever the user has dragged
		// it to. The offset is kept, so the label travels with the line.
		label()->setPos(labelAnchor() + m_labelOffset);
	}
	update();
	ancestorsUpdate();
}

QRectF Arrow::boxRect() const
{
	const QPainterPath path = curve();
	const QRectF r = path.isEmpty() ? QRectF(-2, -2, 4, 4) : path.boundingRect();
	// room for the head, the line and the bend handles, whatever size they are set to
	const qreal margin = qMax(14.0, AppSettings::instance().arrowHeadLength() + 6.0);
	return r.adjusted(-margin, -margin, margin, margin);
}

Arrow::Marks Arrow::markGeometry(const QPainterPath& path) const
{
	Marks marks;
	if (path.isEmpty())
		return marks;

	// An arrow drawn inside something is drawn smaller, and every mark on it
	// by the same step: one number, so it stays in proportion with itself.
	marks.scale = depthScale();
	marks.headLength = AppSettings::instance().arrowHeadLength() * marks.scale;
	marks.headWidth  = AppSettings::instance().arrowHeadWidth()  * marks.scale;

	// The head sits along the curve AS IT ARRIVES, not along the straight
	// line between the two ends: on a bent arrow those point different ways.
	marks.tip = path.pointAtPercent(1.0);
	marks.dir = marks.tip - path.pointAtPercent(0.96);
	const qreal len = norm(marks.dir);
	marks.dir /= (len > 1e-6 ? len : 1);
	marks.normal = QPointF(-marks.dir.y(), marks.dir.x());

	marks.tail = path.pointAtPercent(0.0);
	marks.tailDir = path.pointAtPercent(0.04) - marks.tail;
	const qreal tailLen = norm(marks.tailDir);
	marks.tailDir /= (tailLen > 1e-6 ? tailLen : 1);

	// HOW FAR BACK THE SECOND HEAD OF AN EPI SITS. A stroke runs from the tip
	// along (-L, W); shifting a second copy back by d along the line moves it
	// d*W/hypot(L, W) sideways OF ITSELF, so for a wanted gap g the step back
	// is g*hypot(L, W)/W. See the longer note where this is drawn.
	const qreal headLineWidth = AppSettings::instance().arrowHeadLineWidth() * marks.scale;
	const qreal wantedGap = qMax(2.0 * marks.scale, headLineWidth * 1.5);
	const qreal spread = qMax(0.5, marks.headWidth);
	marks.secondHeadBack = wantedGap
		* qSqrt(marks.headLength * marks.headLength + spread * spread) / spread;
	return marks;
}

QPainterPath Arrow::figure() const
{
	const QPainterPath path = curve();
	if (path.isEmpty())
		return path;
	const Marks m = markGeometry(path);

	// The same shapes paint() draws, in the same places, with no pen on them:
	// what is wanted here is where the arrow IS, not how it is coloured.
	QPainterPath drawn;
	if (drawsDoubleLine())
	{
		QPointF run = m.tip - m.tail;
		const qreal span = norm(run);
		run = span > 1e-6 ? run / span : QPointF(1, 0);
		const qreal gap = doubleLineGap() * m.scale;
		const QPointF sideways(-run.y() * gap, run.x() * gap);
		qreal cut = 0.0;
		if (drawsHead() && m.headWidth > 1e-6)
			cut = m.headLength * gap / m.headWidth;
		const QPainterPath shaft = cut > 0.0 ? trimmedAtEnd(path, cut) : path;
		drawn.addPath(shaft.translated(sideways));
		drawn.addPath(shaft.translated(-sideways));
	}
	else
	{
		drawn.addPath(path);
	}

	const auto stroke = [&drawn](const QPointF& from, const QPointF& to) {
		drawn.moveTo(from);
		drawn.lineTo(to);
	};
	const auto frameAt = [](const QPointF& at, const QPointF& along) {
		QTransform frame;
		frame.translate(at.x(), at.y());
		frame.rotate(qRadiansToDegrees(qAtan2(along.y(), along.x())));
		return frame;
	};

	if (drawsHead())
	{
		stroke(m.tip, m.tip - m.dir * m.headLength + m.normal * m.headWidth);
		stroke(m.tip, m.tip - m.dir * m.headLength - m.normal * m.headWidth);
	}

	switch (m_style)
	{
	case Style::Inclusion:
	{
		const qreal r = qMax(3.0, m.headWidth * 0.8);
		QPainterPath hook;
		hook.moveTo(0, 2 * r);
		hook.cubicTo(-1.34 * r, 2 * r, -1.34 * r, 0, 0, 0);
		drawn.addPath(frameAt(m.tail, m.tailDir).map(hook));
		break;
	}
	case Style::Mono:
	{
		const QTransform frame = frameAt(m.tail, m.tailDir);
		const QPointF vertex = frame.map(QPointF(0, 0));
		stroke(vertex, frame.map(QPointF(-m.headLength, m.headWidth)));
		stroke(vertex, frame.map(QPointF(-m.headLength, -m.headWidth)));
		break;
	}
	case Style::Epi:
	{
		const QPointF back = m.tip - m.dir * m.secondHeadBack;
		stroke(back, back - m.dir * m.headLength + m.normal * m.headWidth);
		stroke(back, back - m.dir * m.headLength - m.normal * m.headWidth);
		break;
	}
	case Style::Iso:
	{
		const QPointF mid = path.pointAtPercent(0.5);
		QPointF midDir = path.pointAtPercent(0.54) - path.pointAtPercent(0.46);
		const qreal midLen = norm(midDir);
		midDir /= (midLen > 1e-6 ? midLen : 1);
		QPointF midNormal(-midDir.y(), midDir.x());
		if (midNormal.y() < 0)
			midNormal = -midNormal;
		QPainterPath tilde;
		tilde.moveTo(-6, 2);
		tilde.cubicTo(-4, -3, -2, -3, 0, 0);
		tilde.cubicTo(2, 3, 4, 3, 6, -2);
		drawn.addPath(frameAt(mid + midNormal * 9.0, midDir).map(tilde));
		break;
	}
	case Style::Equals:
	case Style::Plain:
		break;
	}

	// asserted, as against drawn as a style: an arrow told it is epic both
	// ways carries both marks, and both are part of it
	if (isEpic())
	{
		const QPointF back = m.tip - m.dir * m.secondHeadBack;
		stroke(back, back - m.dir * m.headLength + m.normal * m.headWidth);
		stroke(back, back - m.dir * m.headLength - m.normal * m.headWidth);
	}
	if (isMonic())
	{
		const QPointF tailNormal(-m.tailDir.y(), m.tailDir.x());
		if (isInclusion())
		{
			const qreal r = qMax(3.0, m.headWidth * 0.8);
			QPainterPath hook;
			hook.moveTo(0, 2 * r);
			hook.cubicTo(-1.34 * r, 2 * r, -1.34 * r, 0, 0, 0);
			drawn.addPath(frameAt(m.tail, m.tailDir).map(hook));
		}
		else
		{
			const QPointF back = m.tail - m.tailDir * m.headLength;
			stroke(m.tail, back + tailNormal * m.headWidth);
			stroke(m.tail, back - tailNormal * m.headWidth);
		}
	}
	return drawn;
}

QRectF Arrow::boundingRect() const
{
	// the line together with the label, which hangs beside it and so is very
	// nearly always outside boxRect()
	return boxRect() | childFrame();
}

QPainterPath Arrow::shape() const
{
	// AN ARROW IS ITS LINE AND ITS MARKS.
	//
	// This used to stroke the curve alone, so a press, a double-click or a
	// right-click that landed on the head - or on the hook of an inclusion,
	// or on the second head of an epi - went straight past the arrow to
	// whatever was behind it. The whole drawn figure is stroked instead, each
	// piece of it fattened by the same "how near counts as on it" width, and
	// the overlapping strokes are merged into one outline so the result is a
	// single region rather than a stack of them.
	const QPainterPath drawn = figure();
	if (drawn.isEmpty())
		return drawn;
	QPainterPathStroker stroker;
	stroker.setWidth(AppSettings::instance().arrowHitWidth());   // how near counts as on it
	stroker.setCapStyle(Qt::RoundCap);
	stroker.setJoinStyle(Qt::RoundJoin);
	QPainterPath hit = stroker.createStroke(drawn).simplified();
	for (const QPointF& bend : m_bends)
		hit.addEllipse(bend, 9, 9);   // and its bends are grabbable
	return hit;
}

void Arrow::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	Node::paint(painter, option, widget);
	const QPainterPath path = curve();
	if (path.isEmpty())
		return;

	// An arrow drawn inside something is drawn lighter and smaller than one on
	// the canvas, and by the same step as everything else at that depth: the
	// line, the head, and whatever mark the style puts on the tail all come
	// off this one number, so an arrow stays in proportion with itself.
	// where every mark on this arrow goes, worked out once and shared with
	// shape(), so what is drawn and what can be clicked are the same figure
	const Marks marks = markGeometry(path);
	const qreal scale = marks.scale;

	QPen pen = border();
	if (pen.style() == Qt::NoPen)
		pen = QPen(Qt::black, 1.5 * scale);
	// the width is a setting, unless this arrow was given a colour by hand
	if (!hasChosenStyle())
		pen.setWidthF(AppSettings::instance().arrowLineWidth() * scale);
	// round ends, so the tail is not a little square nub and a bend in the
	// curve does not show a corner where two segments meet
	pen.setCapStyle(Qt::RoundCap);
	pen.setJoinStyle(Qt::RoundJoin);
	if (existsSuch())
		pen.setStyle(Qt::DotLine);   // dotted along its line: this arrow is asserted to exist
	if (hasError())
	{
		const Qt::PenStyle style = pen.style();
		pen = QPen(QColor(255, 0, 0), 2.5 * scale);
		pen.setStyle(style);
	}
	if (isHighlighted())
	{
		const Qt::PenStyle style = pen.style();
		pen = QPen(QColor(22, 163, 74), qMax(3.0 * scale, pen.widthF() + 1.0 * scale));
		pen.setStyle(style);
	}
	const bool selected = (option->state & QStyle::State_Selected) != 0;
	if (selected)
		pen.setWidthF(pen.widthF() + 1.5 * scale);
	painter->setPen(pen);
	painter->setBrush(Qt::NoBrush);
	const qreal headLengthAhead = marks.headLength;
	const qreal headWidthAhead = marks.headWidth;

	if (drawsDoubleLine())
	{
		// The equals sign as it is written: two lines side by side. The path
		// is drawn twice, shifted either way ACROSS the line it runs along, so
		// the pair reads as one mark rather than as two arrows.
		QPointF run = path.pointAtPercent(1.0) - path.pointAtPercent(0.0);
		const qreal span = norm(run);
		run = span > 1e-6 ? run / span : QPointF(1, 0);
		const qreal gap = doubleLineGap() * scale;
		const QPointF sideways(-run.y() * gap, run.x() * gap);

		// EACH LINE STOPS WHERE THE HEAD CROSSES IT.
		//
		// A single line ends at the tip, which is where the two strokes of the
		// head meet, so it has nothing to overshoot. A doubled line does not:
		// its two halves run either side of the tip and carry on past the
		// strokes, which cross them at an angle and leave two whiskers poking
		// out of the head.
		//
		// Where they cross is exact, not a guess. A stroke leaves the tip
		// along (-dir*L + normal*W), so the point on it that is `gap` off to
		// the side is the one at L*gap/W back along the line - and that is the
		// same distance for both halves, the figure being symmetric. So take
		// that much off the far end of each.
		qreal cut = 0.0;
		if (drawsHead() && headWidthAhead > 1e-6)
			cut = headLengthAhead * gap / headWidthAhead;
		const QPainterPath shaft = cut > 0.0 ? trimmedAtEnd(path, cut) : path;
		painter->drawPath(shaft.translated(sideways));
		painter->drawPath(shaft.translated(-sideways));
	}
	else
	{
		painter->drawPath(path);
	}

	// the head: a filled triangle at the codomain end, along the curve as it
	// arrives rather than along the straight line between the ends
	const QPointF tip = marks.tip;
	const QPointF dir = marks.dir;
	const QPointF normal = marks.normal;
	// Two strokes back from the tip, not a filled triangle: an arrowhead is
	// drawn, not blocked in. The spread and the length together are its angle.
	const qreal headLength = headLengthAhead;
	const qreal headWidth = headWidthAhead;
	// Selected thickens the head and whatever tail mark the style draws, not
	// just the line: an arrow is its line AND its marks, and a fat line with a
	// hairline hook reads as a mistake rather than as a selection.
	// HOW FAR BACK THE SECOND HEAD OF AN EPI SITS.
	//
	// Not a fixed fraction of the head's length: what the eye reads is the gap
	// between the two strokes measured ACROSS them, and that depends on how
	// open the head is. A wide head's strokes lie nearly across the line, so a
	// small step back already separates them; a narrow head's lie nearly along
	// it and need a long one. Stepping back a fixed amount made a narrow head
	// look like one thick mark and a wide head like two unrelated ones.
	//
	// A stroke runs from the tip along (-L, W). Shifting a second copy back by
	// d along the line moves it d*W/hypot(L, W) sideways OF ITSELF, so for a
	// wanted gap g the step back is g*hypot(L, W)/W.
	const qreal headLineWidth = AppSettings::instance().arrowHeadLineWidth() * scale;
	const qreal secondHeadBack = marks.secondHeadBack;

	QPen headPen(pen.color(),
	             headLineWidth + (selected ? 1.5 * scale : 0.0),
	             Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
	painter->setPen(headPen);
	painter->setBrush(Qt::NoBrush);
	// An equals points nowhere: it says the two ends are the same thing, and a
	// head on it would read as a map from one to the other.
	if (drawsHead())
	{
		painter->drawLine(tip, tip - dir * headLength + normal * headWidth);
		painter->drawLine(tip, tip - dir * headLength - normal * headWidth);
	}

	// What KIND of arrow this is, said in the usual marks. Everything below is
	// drawn in a frame standing on the line with +x running ALONG it and +y
	// across it, so each shape can be written as if the arrow were horizontal
	// and still comes out the right way round at any angle.
	if (m_style != Style::Plain)
	{
		const auto frameAt = [](const QPointF& at, const QPointF& along) {
			QTransform frame;
			frame.translate(at.x(), at.y());
			frame.rotate(qRadiansToDegrees(qAtan2(along.y(), along.x())));
			return frame;
		};

		const QPointF tail = marks.tail;
		const QPointF tailDir = marks.tailDir;   // into the line

		painter->setBrush(Qt::NoBrush);
		switch (m_style)
		{
		case Style::Inclusion:
		{
			// the hook of the inclusion sign: a half circle standing on the
			// tail and curling BACK, away from where the arrow is going
			const qreal r = qMax(3.0, headWidth * 0.8);
			QPainterPath hook;
			hook.moveTo(0, 2 * r);
			hook.cubicTo(-1.34 * r, 2 * r, -1.34 * r, 0, 0, 0);
			painter->drawPath(frameAt(tail, tailDir).map(hook));
			break;
		}
		case Style::Mono:
		{
			// A barb whose point is where the line begins, opening backwards
			// towards the domain - the same shape the Monic prop draws, and
			// for the same reason the line does not run through it.
			const QTransform frame = frameAt(tail, tailDir);
			const QPointF vertex = frame.map(QPointF(0, 0));
			painter->drawLine(vertex, frame.map(QPointF(-headLength, headWidth)));
			painter->drawLine(vertex, frame.map(QPointF(-headLength, -headWidth)));
			break;
		}
		case Style::Epi:
		{
			// a second head, far enough back to read as two (see secondHeadBack)
			const QPointF back = tip - dir * secondHeadBack;
			painter->drawLine(back, back - dir * headLength + normal * headWidth);
			painter->drawLine(back, back - dir * headLength - normal * headWidth);
			break;
		}
		case Style::Iso:
		{
			// a tilde over the line - on the side the LABEL is not, since
			// labelAnchor() hangs the name above the middle and the two would
			// otherwise sit on top of each other
			const QPointF mid = path.pointAtPercent(0.5);
			QPointF midDir = path.pointAtPercent(0.54) - path.pointAtPercent(0.46);
			const qreal midLen = qSqrt(midDir.x() * midDir.x() + midDir.y() * midDir.y());
			midDir /= (midLen > 1e-6 ? midLen : 1);
			QPointF midNormal(-midDir.y(), midDir.x());
			if (midNormal.y() < 0)
				midNormal = -midNormal;
			QPainterPath tilde;
			tilde.moveTo(-6, 2);
			tilde.cubicTo(-4, -3, -2, -3, 0, 0);
			tilde.cubicTo(2, 3, 4, 3, 6, -2);
			painter->drawPath(frameAt(mid + midNormal * 9.0, midDir).map(tilde));
			break;
		}
		case Style::Equals:
		case Style::Plain:
			break;   // the doubled line above is the whole of an equals
		}
	}


	// Asserted epic (props/ArrowProps.h): a second chevron stacked back along
	// the line, the way a quotient is usually drawn. This is the OTHER of the
	// two mechanisms - Style::Epi above says the same thing as a drawn mark -
	// so an arrow told it is epic both ways is drawn with both.
	if (isEpic())
	{
		const QPointF tip2 = tip - dir * secondHeadBack;
		painter->drawLine(tip2, tip2 - dir * headLength + normal * headWidth);
		painter->drawLine(tip2, tip2 - dir * headLength - normal * headWidth);
	}

	// asserted monic: a vee at the tail, its apex pointing the way the arrow
	// goes, so the tail reads as a mirror of the head rather than a bar across it
	if (isMonic())
	{
		const QPointF start = marks.tail;
		const QPointF tailWay = marks.tailDir;
		const QPointF tailNormal(-tailWay.y(), tailWay.x());

		if (isInclusion())
		{
			// a hook, curling back off the end of the line: the line runs INTO
			// it, which is why no room was made for it in throughPoints
			const qreal r = qMax(3.0, headWidth * 0.8);
			QTransform frame;
			frame.translate(start.x(), start.y());
			frame.rotate(qRadiansToDegrees(qAtan2(tailWay.y(), tailWay.x())));
			QPainterPath hook;
			hook.moveTo(0, 2 * r);
			hook.cubicTo(-1.34 * r, 2 * r, -1.34 * r, 0, 0, 0);
			painter->setBrush(Qt::NoBrush);
			painter->drawPath(frame.map(hook));
		}
		else
		{
			// the point of the vee IS where the line begins (throughPoints made
			// room for it); the arms reach back from there towards the domain
			const QPointF back = start - tailWay * headLength;
			painter->drawLine(start, back + tailNormal * headWidth);
			painter->drawLine(start, back - tailNormal * headWidth);
		}
	}

	// Struck off: this arrow goes when the rule is applied. On the middle of
	// its own line, where the eye already is.
	if (markedForDeletion())
	{
		const QPointF at = path.pointAtPercent(0.5);
		const qreal reach = 8.0;
		painter->setBrush(Qt::NoBrush);
		painter->setPen(QPen(QColor(220, 38, 38), 2.5, Qt::SolidLine, Qt::RoundCap));
		painter->drawLine(at + QPointF(-reach, -reach), at + QPointF(reach, reach));
		painter->drawLine(at + QPointF(-reach, reach), at + QPointF(reach, -reach));
	}

	// the points it is pulled through, while it is being worked on
	if (!m_bends.isEmpty() && ((option->state & QStyle::State_Selected) || m_hovered))
	{
		painter->setPen(QPen(Qt::white, 1.2));
		painter->setBrush(QColor(99, 102, 241, 235));
		for (const QPointF& bend : m_bends)
			painter->drawEllipse(bend, 4.5, 4.5);
	}
}

// ---------------------------------------------------------------- properties

QStringList Arrow::properties() const
{
	QStringList keys;
	for (const ArrowProp* p : m_props)
		keys << p->key();
	return keys;
}

bool Arrow::has(const QString& key) const
{
	return prop(key) != nullptr;
}

ArrowProp* Arrow::prop(const QString& key) const
{
	for (ArrowProp* p : m_props)
		if (p->key() == key)
			return p;
	return nullptr;
}

void Arrow::addProperty(const QString& key)
{
	if (has(key))
		return;
	if (ArrowProp* p = ArrowProp::create(key, this))   // owned by us
		m_props.append(p);
	else
		qWarning("Arrow '%s': unknown property '%s'", qPrintable(id()), qPrintable(key));
}

void Arrow::setProperties(const QStringList& keys)
{
	qDeleteAll(m_props);
	m_props.clear();
	for (const QString& key : keys)
		addProperty(key);
}


QString Arrow::effectiveId() const
{
	if (!id().isEmpty())
		return id();
	Category* home = surroundingCategory();
	return home != nullptr ? home->implicitArrowName() : QString();
}

QString Arrow::contextTitle() const
{
	Category* home = surroundingCategory();
	if (home == nullptr)
		return Node::contextTitle();
	// an implicit name is shown as such, so it is clear the label is blank
	const QString name = id().isEmpty() && !effectiveId().isEmpty()
		? QString("%1 (implicit)").arg(effectiveId())
		: id();
	return QString("%1 %2").arg(sentenceCase(home->morphismName()), name);
}

void Arrow::populateActions(QMenu& menu)
{
	// THE ONE THING HERE THAT IS ABOUT THE POINT YOU CLICKED.
	//
	// A bend goes WHERE the cursor is, and taking one out means taking out
	// the one under the cursor - neither question can be asked anywhere but
	// here, because nowhere else knows the place. So this stays, and it is
	// all that stays.
	//
	// What this arrow is claimed to be (monic, epic), what it is drawn as
	// (Style), its colours, and what it does to the elements of its domain
	// are all properties of the arrow rather than of the spot on its line,
	// and are asked in the Properties panel. Straightening needs no place
	// either, so it is there too. See Node::contextMenuEvent.
	const QPointF at = contextPos();
	const int index = bendAt(at);
	if (index >= 0)
	{
		QAction* remove = menu.addAction("Delete bend point");
		remove->setToolTip("Take out the bend under the cursor. Straighten, on the Properties panel, "
		                   "takes out all of them.");
		QObject::connect(remove, &QAction::triggered, &menu, [this, index] {
			const QList<QPointF> before = m_bends;
			removeBend(index);
			recordBends(QString("Took a bend out of %1").arg(id()), before);
		});
	}
	else
	{
		QAction* add = menu.addAction("Add a bend here");
		add->setToolTip("Pull the line through this point. Drag the line to bend it by hand.");
		QObject::connect(add, &QAction::triggered, &menu, [this, at] {
			const QList<QPointF> before = m_bends;
			addBend(at);
			recordBends(QString("Bent %1").arg(id()), before);
		});
	}
	menu.addSeparator();
}
// ---------------------------------------------------------------- what kind of arrow

QList<Arrow::Style> Arrow::allStyles()
{
	return { Style::Plain, Style::Inclusion, Style::Mono, Style::Epi, Style::Iso };
}

QString Arrow::styleName(Style style)
{
	switch (style)
	{
	case Style::Inclusion: return QStringLiteral("Inclusion  ↪");
	case Style::Mono:      return QStringLiteral("Monomorphism  ↣");
	case Style::Epi:       return QStringLiteral("Epimorphism  ↠");
	case Style::Iso:       return QStringLiteral("Isomorphism  ≃");
	case Style::Equals:    return QStringLiteral("Equals  =");
	case Style::Plain:     break;
	}
	return QStringLiteral("Plain  →");
}

QString Arrow::styleDescription(Style style)
{
	switch (style)
	{
	case Style::Inclusion:
		return QStringLiteral("A hooked tail: this arrow takes one thing into another it is part of.");
	case Style::Mono:
		return QStringLiteral("A barbed tail: monic, cancellable on the left. If this after two arrows "
		                      "gives the same answer either way, those two arrows were equal.");
	case Style::Epi:
		return QStringLiteral("Two heads: epic, cancellable on the right. If two arrows after this give "
		                      "the same answer either way, those two arrows were equal.");
	case Style::Iso:
		return QStringLiteral("A tilde over the line: invertible. There is an arrow back the other way, "
		                      "and both composites are identities.");
	case Style::Equals:
		return QStringLiteral("Two lines and no head: the thing at one end IS the thing at the other. "
		                      "Not a map between them - an equals reads the same way round either way.");
	case Style::Plain:
		break;
	}
	return QStringLiteral("A plain arrow: nothing claimed about it beyond its being an arrow.");
}

void Arrow::setStyle(Style style)
{
	if (m_style == style)
		return;
	// the hook and the barb stand off the end of the line, so the rect has to
	// be allowed to grow before they are drawn
	prepareGeometryChange();
	ancestorsPrepareGeometryChange();
	m_style = style;
	update();
	ancestorsUpdate();
	emit styleChanged(this);
}

void Arrow::setStyleRecorded(Style style)
{
	if (m_style == style)
		return;
	const Style before = m_style;
	setStyle(style);
	if (auto* diagram = diagramOf(this))
		diagram->noteArrowStyle(this, before, style);
}

// ---------------------------------------------------------------- bending it by hand

qreal Arrow::fractionAlong(const QPointF& at) const
{
	const QPainterPath path = curve();
	if (path.isEmpty())
		return 0.5;
	// Sampled rather than solved: a cubic has no tidy nearest-point, and 64
	// steps along a line a few hundred units long is finer than a mouse.
	const int steps = 64;
	qreal nearest = 0.5;
	qreal best = -1;
	for (int i = 0; i <= steps; ++i)
	{
		const qreal t = qreal(i) / steps;
		const qreal d = norm(path.pointAtPercent(t) - at);
		if (best < 0 || d < best)
		{
			best = d;
			nearest = t;
		}
	}
	return nearest;
}

bool Arrow::takesPressAt(const QPointF& itemPos) const
{
	if (bendAt(itemPos) >= 0)
		return true;   // a point already placed is grabbable wherever it sits
	const qreal along = fractionAlong(itemPos);
	return along >= BendFreeEnds && along <= 1.0 - BendFreeEnds;
}

void Arrow::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
	if (event->button() != Qt::LeftButton)
	{
		Node::mousePressEvent(event);
		return;
	}

	// Near either end this press is not ours: ignored rather than swallowed,
	// so the scene hands it on to whatever is under it and the node moves as
	// it was asked to.
	if (!takesPressAt(event->pos()))
	{
		event->ignore();
		return;
	}

	m_bendsAtPress = m_bends;
	m_dragBend = bendAt(event->pos());
	m_pressPos = event->pos();
	m_pressed = true;
	// we take this press to bend the line, so Qt's own selection handling
	// never runs: do the part of it the user expects
	if (scene() != nullptr && !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)))
		scene()->clearSelection();
	setSelected(true);
	event->accept();
}

void Arrow::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
	if (!m_pressed)
	{
		Node::mouseMoveEvent(event);
		return;
	}
	// Below this it is a click with a shaky hand, not a drag - and that must
	// hold for moving an EXISTING bend too, or a click on one would snap it to
	// the nearest grid point.
	if (QLineF(m_pressPos, event->pos()).length() < 3)
		return;

	if (m_dragBend < 0)
	{
		// the line itself was pulled: a point appears where it was grabbed,
		// and the rest of this drag moves that point
		QList<QPointF> bends = m_bends;
		m_dragBend = qBound(0, bendIndexFor(m_pressPos), int(bends.size()));
		bends.insert(m_dragBend, m_pressPos);
		setBends(bends);
	}
	if (m_dragBend >= 0 && m_dragBend < m_bends.size())
	{
		QList<QPointF> bends = m_bends;
		// on the same hidden grid as everything else
		bends[m_dragBend] = mapFromParent(snapped(mapToParent(event->pos())));
		// through setBends, which ANNOUNCES the new shape: an arrow that ends
		// on this one, and the handle bar sitting beside it, both follow
		setBends(bends);
	}
	event->accept();
}

void Arrow::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
	// Pulled back onto the line it would have taken anyway: the point is not
	// holding the curve in any shape, so it goes. Otherwise an arrow that
	// LOOKS straight quietly keeps a control point, and the next drag of an
	// endpoint bends it for no visible reason.
	if (m_dragBend >= 0 && m_dragBend < m_bends.size())
	{
		QList<QPointF> bends = m_bends;
		bends.removeAt(m_dragBend);
		if (isRedundantBend(m_bends.at(m_dragBend), bends))
		{
			setBends(bends);
			m_dragBend = -1;
		}
	}

	if (m_pressed)
		recordBends(QString("Bent %1").arg(id()), m_bendsAtPress);
	m_pressed = false;
	m_dragBend = -1;
	Node::mouseReleaseEvent(event);
}

void Arrow::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
	m_hovered = true;
	update();
	Node::hoverEnterEvent(event);
}

void Arrow::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
	m_hovered = false;
	update();
	Node::hoverLeaveEvent(event);
}

bool Arrow::isRedundantBend(const QPointF& point, const QList<QPointF>& without) const
{
	// Inside one of the ends: invisible, and ruinous to the line (see
	// throughPoints). Let go of it there and it is let go of for good.
	if (m_domain != nullptr && endFrame(m_domain).contains(point))
		return true;
	if (m_codomain != nullptr && endFrame(m_codomain).contains(point))
		return true;

	// the shape this arrow would have if that point were not there
	Arrow* self = const_cast<Arrow*>(this);
	const QList<QPointF> had = m_bends;
	self->m_bends = without;
	const QPainterPath plain = curve();
	self->m_bends = had;

	if (plain.isEmpty())
		return false;

	// how far the point sits from that shape, at its nearest
	qreal nearest = -1;
	for (int i = 0; i <= 64; ++i)
	{
		const QPointF d = plain.pointAtPercent(i / 64.0) - point;
		const qreal distance = QLineF(QPointF(0, 0), d).length();
		if (nearest < 0 || distance < nearest)
			nearest = distance;
	}
	// within a couple of pixels is on it, as far as the eye is concerned
	return nearest >= 0 && nearest < 3.0;
}

// ---------------------------------------------------------------- what it is
// asserted to be

void Arrow::removeProperty(const QString& key)
{
	for (int i = 0; i < m_props.size(); ++i)
	{
		if (m_props.at(i)->key() == key)
		{
			delete m_props.takeAt(i);
			return;
		}
	}
}

bool Arrow::isMonic() const
{
	// by type: an Inclusion IS a Monomorphism, and answers yes here
	return propOfType<Monomorphism>() != nullptr;
}

bool Arrow::isInclusion() const
{
	return propOfType<Inclusion>() != nullptr;
}

bool Arrow::isEpic() const
{
	return has(Epimorphism::Key());
}

void Arrow::setMonic(bool monic)
{
	if (monic == isMonic())
		return;
	if (monic)
	{
		addProperty(Monomorphism::Key());
	}
	else
	{
		// take off whichever kind of monomorphism it is wearing: saying it is
		// not monic has to mean it is not an inclusion either
		removeProperty(Monomorphism::Key());
		removeProperty(Inclusion::Key());
	}
	update();
	emit styleChanged(this);
}

void Arrow::setInclusion(bool inclusion)
{
	if (inclusion == isInclusion())
		return;
	if (inclusion)
	{
		// One property, not two: an inclusion already IS a monomorphism, and
		// carrying both would let them be turned off separately and disagree.
		removeProperty(Monomorphism::Key());
		addProperty(Inclusion::Key());
	}
	else
	{
		// it stays monic - only the narrower claim is being given up
		removeProperty(Inclusion::Key());
		addProperty(Monomorphism::Key());
	}
	update();
	emit styleChanged(this);
}

void Arrow::setInclusionRecorded(bool inclusion)
{
	if (inclusion == isInclusion())
		return;
	const QString name = id().isEmpty() ? QStringLiteral("an arrow") : id();
	setInclusion(inclusion);
	if (auto* diagram = diagramOf(this))
		diagram->history()->record(new MonicChanged(
			inclusion ? QString("%1 is asserted an inclusion").arg(name)
			          : QString("%1 is a monomorphism, no longer an inclusion").arg(name),
			this, !inclusion, inclusion));
}

void Arrow::setEpic(bool epic)
{
	if (epic == isEpic())
		return;
	if (epic) addProperty(Epimorphism::Key());
	else removeProperty(Epimorphism::Key());
	update();
	emit styleChanged(this);
}

void Arrow::setMonicRecorded(bool monic)
{
	if (monic == isMonic())
		return;
	const QString name = id().isEmpty() ? QStringLiteral("an arrow") : id();
	setMonic(monic);
	if (auto* diagram = diagramOf(this))
		diagram->history()->record(new MonicChanged(
			monic ? QString("%1 is asserted a monomorphism").arg(name)
			      : QString("%1 is no longer asserted a monomorphism").arg(name),
			this, !monic, monic));
}

void Arrow::setEpicRecorded(bool epic)
{
	if (epic == isEpic())
		return;
	const QString name = id().isEmpty() ? QStringLiteral("an arrow") : id();
	setEpic(epic);
	if (auto* diagram = diagramOf(this))
		diagram->history()->record(new EpicChanged(
			epic ? QString("%1 is asserted an epimorphism").arg(name)
			     : QString("%1 is no longer asserted an epimorphism").arg(name),
			this, !epic, epic));
}
