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
	// half the space between the two lines of an equals, in scene units
	const qreal kEqualsGap = 1.8;

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

	QPointF start, end;
	if (m_bends.isEmpty())
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
		start = attachTo(rd, m_bends.first());
		end = attachTo(rc, m_bends.last());

		QPointF out = m_bends.first() - start;
		qreal length = norm(out);
		if (length > 1e-6)
			start += (out / length) * 3;
		QPointF in = end - m_bends.last();
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
		const QPointF toward = m_bends.isEmpty() ? end : m_bends.first();
		QPointF along = toward - start;
		const qreal length = norm(along);
		const qreal room = AppSettings::instance().arrowHeadLength();
		if (length > room * 1.5)
			start += (along / length) * room;
	}

	pts << start;
	pts << m_bends;
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
			normal = -normal;   // above the line
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
	m_labelOffset = offset;
	rememberLabelPlacement();
	refreshGeometry();
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
		emit labelDragged(this, m_labelOffset - was);
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

QRectF Arrow::boundingRect() const
{
	// the line together with the label, which hangs beside it and so is very
	// nearly always outside boxRect()
	return boxRect() | childFrame();
}

QPainterPath Arrow::shape() const
{
	const QPainterPath path = curve();
	if (path.isEmpty())
		return path;
	QPainterPathStroker stroker;
	stroker.setWidth(AppSettings::instance().arrowHitWidth());   // how near counts as on it
	QPainterPath hit = stroker.createStroke(path);
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

	QPen pen = border();
	if (pen.style() == Qt::NoPen)
		pen = QPen(Qt::black, 1.5);
	// the width is a setting, unless this arrow was given a colour by hand
	if (!hasChosenStyle())
		pen.setWidthF(AppSettings::instance().arrowLineWidth());
	// round ends, so the tail is not a little square nub and a bend in the
	// curve does not show a corner where two segments meet
	pen.setCapStyle(Qt::RoundCap);
	pen.setJoinStyle(Qt::RoundJoin);
	if (existsSuch())
		pen.setStyle(Qt::DotLine);   // dotted along its line: this arrow is asserted to exist
	if (hasError())
	{
		const Qt::PenStyle style = pen.style();
		pen = QPen(QColor(255, 0, 0), 2.5);
		pen.setStyle(style);
	}
	if (isHighlighted())
	{
		const Qt::PenStyle style = pen.style();
		pen = QPen(QColor(22, 163, 74), qMax(3.0, pen.widthF() + 1.0));
		pen.setStyle(style);
	}
	const bool selected = (option->state & QStyle::State_Selected) != 0;
	if (selected)
		pen.setWidthF(pen.widthF() + 1.5);
	painter->setPen(pen);
	painter->setBrush(Qt::NoBrush);
	if (m_style == Style::Equals)
	{
		// The equals sign as it is written: two lines side by side. The path
		// is drawn twice, shifted either way ACROSS the line it runs along, so
		// the pair reads as one mark rather than as two arrows.
		QPointF run = path.pointAtPercent(1.0) - path.pointAtPercent(0.0);
		const qreal span = norm(run);
		run = span > 1e-6 ? run / span : QPointF(1, 0);
		const QPointF sideways(-run.y() * kEqualsGap, run.x() * kEqualsGap);
		painter->drawPath(path.translated(sideways));
		painter->drawPath(path.translated(-sideways));
	}
	else
	{
		painter->drawPath(path);
	}

	// the head: a filled triangle at the codomain end, along the curve as it
	// arrives rather than along the straight line between the ends
	const QPointF tip = path.pointAtPercent(1.0);
	QPointF dir = tip - path.pointAtPercent(0.96);
	const qreal len = qSqrt(dir.x() * dir.x() + dir.y() * dir.y());
	dir /= (len > 1e-6 ? len : 1);
	const QPointF normal(-dir.y(), dir.x());
	// Two strokes back from the tip, not a filled triangle: an arrowhead is
	// drawn, not blocked in. The spread and the length together are its angle.
	const qreal headLength = AppSettings::instance().arrowHeadLength();
	const qreal headWidth = AppSettings::instance().arrowHeadWidth();
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
	const qreal wantedGap = qMax(2.0, AppSettings::instance().arrowHeadLineWidth() * 1.5);
	const qreal spread = qMax(0.5, headWidth);
	const qreal secondHeadBack = wantedGap * qSqrt(headLength * headLength + spread * spread) / spread;

	QPen headPen(pen.color(),
	             AppSettings::instance().arrowHeadLineWidth() + (selected ? 1.5 : 0.0),
	             Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
	painter->setPen(headPen);
	painter->setBrush(Qt::NoBrush);
	// An equals points nowhere: it says the two ends are the same thing, and a
	// head on it would read as a map from one to the other.
	if (m_style != Style::Equals)
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

		const QPointF tail = path.pointAtPercent(0.0);
		QPointF tailDir = path.pointAtPercent(0.04) - tail;   // into the line
		const qreal tailLen = qSqrt(tailDir.x() * tailDir.x() + tailDir.y() * tailDir.y());
		tailDir /= (tailLen > 1e-6 ? tailLen : 1);

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
		const QPointF start = path.pointAtPercent(0.0);
		QPointF tailWay = path.pointAtPercent(0.04) - start;
		const qreal len = qSqrt(tailWay.x() * tailWay.x() + tailWay.y() * tailWay.y());
		tailWay /= (len > 1e-6 ? len : 1);
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
	// what it does: carrying the elements of its domain over, and whatever
	// else its properties offer
	for (ArrowProp* p : m_props)
		p->arrowContextMenu(menu, this);

	// What this arrow is asserted to be, cancellable on the left or the right
	// (or both). Checkable, like Exists such: an ASSERTION, not a construction.
	// The Style submenu below says overlapping things with a drawn mark; both
	// mechanisms are kept, and they are free to disagree.
	Arrow* self = this;
	QAction* monic = menu.addAction(Emoji::monomorphism() + "  Monomorphism");
	monic->setCheckable(true);
	monic->setChecked(isMonic());
	monic->setToolTip(QString("Cancellable on the left: for g, h : Z %1 X, f%2g = f%2h implies g = h. "
	                         "Drawn with a vee at the tail.")
		.arg(Emoji::to(), Emoji::compose()));
	QObject::connect(monic, &QAction::toggled, &menu, [self](bool on) { self->setMonicRecorded(on); });

	QAction* epic = menu.addAction(Emoji::epimorphism() + "  Epimorphism");
	epic->setCheckable(true);
	epic->setChecked(isEpic());
	epic->setToolTip(QString("Cancellable on the right: for g, h : Y %1 Z, g%2f = h%2f implies g = h. "
	                        "Drawn with a doubled head.")
		.arg(Emoji::to(), Emoji::compose()));
	QObject::connect(epic, &QAction::toggled, &menu, [self](bool on) { self->setEpicRecorded(on); });
	menu.addSeparator();

	// what kind of arrow this is claimed to be
	QMenu* style = menu.addMenu(Emoji::to() + "  Style");
	auto* styles = new QActionGroup(style);
	styles->setExclusive(true);
	for (Style option : allStyles())
	{
		QAction* act = style->addAction(styleName(option));
		act->setCheckable(true);
		act->setChecked(m_style == option);
		act->setToolTip(styleDescription(option));
		styles->addAction(act);
		QObject::connect(act, &QAction::triggered, style, [this, option] { setStyleRecorded(option); });
	}

	// the shape of the line. No asking twice about a bend - the history has
	// it either way.
	QMenu* shape = menu.addMenu(Emoji::shape() + "  Shape");
	const QPointF at = contextPos();
	const int index = bendAt(at);
	if (index >= 0)
	{
		QAction* remove = shape->addAction("Delete bend point");
		QObject::connect(remove, &QAction::triggered, shape, [this, index] {
			const QList<QPointF> before = m_bends;
			removeBend(index);
			recordBends(QString("Took a bend out of %1").arg(id()), before);
		});
	}
	else
	{
		QAction* add = shape->addAction("Add a bend here");
		QObject::connect(add, &QAction::triggered, shape, [this, at] {
			const QList<QPointF> before = m_bends;
			addBend(at);
			recordBends(QString("Bent %1").arg(id()), before);
		});
	}
	QAction* straighten = shape->addAction("Straighten");
	straighten->setEnabled(!m_bends.isEmpty());
	QObject::connect(straighten, &QAction::triggered, shape, [this] {
		const QList<QPointF> before = m_bends;
		this->straighten();
		recordBends(QString("Straightened %1").arg(id()), before);
	});

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

void Arrow::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
	if (event->button() != Qt::LeftButton)
	{
		Node::mousePressEvent(event);
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
