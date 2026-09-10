#include "Arrow.h"

#include <QMenu>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include "props/ArrowProp.h"
#include "props/ArrowProps.h"
#include "DiagramScene.h"
#include "Category.h"
#include "AppSettings.h"
#include "Emoji.h"
#include "history/SceneHistory.h"
#include "history/Mementos.h"
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
	setBorder(QPen(Qt::black, 1.5));
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
	return mapFromItem(end, end->boundingRect()).boundingRect();
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

QList<QPointF> Arrow::throughPoints() const
{
	QList<QPointF> pts;
	if (m_domain == nullptr || m_codomain == nullptr)
		return pts;
	const QRectF rd = endFrame(m_domain);
	const QRectF rc = endFrame(m_codomain);

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
		middle = (mapFromItem(m_domain, m_domain->boundingRect().center())
		        + mapFromItem(m_codomain, m_codomain->boundingRect().center())) / 2;
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

void Arrow::placeLabel()
{
	if (label() != nullptr)
		label()->setPos(labelAnchor() + m_labelOffset);
}

void Arrow::setLabelOffset(const QPointF& offset)
{
	if (m_labelOffset == offset)
		return;
	m_labelOffset = offset;
	refreshGeometry();
}

void Arrow::labelMoved(const QPointF& pos)
{
	// what the user dragged is remembered as a displacement, so the label
	// travels with the line rather than staying where the screen was
	m_labelOffset = pos - labelAnchor();
	prepareGeometryChange();
	ancestorsPrepareGeometryChange();
	update();
	ancestorsUpdate();
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

QRectF Arrow::boundingRect() const
{
	const QPainterPath path = curve();
	const QRectF r = path.isEmpty() ? QRectF(-2, -2, 4, 4) : path.boundingRect();
	// room for the head, the line and the bend handles, whatever size they are set to
	const qreal margin = qMax(14.0, AppSettings::instance().arrowHeadLength() + 6.0);
	return r.adjusted(-margin, -margin, margin, margin) | childFrame();
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
	if (hasError())
		pen = QPen(QColor(255, 0, 0), 2.5);
	if (isHighlighted())
		pen = QPen(QColor(22, 163, 74), qMax(3.0, pen.widthF() + 1.0));
	if (option->state & QStyle::State_Selected)
		pen.setWidthF(pen.widthF() + 1.5);
	// Dashed along its line: this arrow is asserted to exist. Applied LAST, once
	// an error or a highlight has had its say about the colour and the width -
	// the dash is measured against the width the line is finally drawn with,
	// and a fresh pen would not carry the pattern over anyway.
	if (existsSuch())
		applyExistsDash(pen);
	painter->setPen(pen);
	painter->setBrush(Qt::NoBrush);
	painter->drawPath(path);

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
	QPen headPen(pen.color(), AppSettings::instance().arrowHeadLineWidth(),
	             Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
	painter->setPen(headPen);
	painter->setBrush(Qt::NoBrush);
	painter->drawLine(tip, tip - dir * headLength + normal * headWidth);
	painter->drawLine(tip, tip - dir * headLength - normal * headWidth);

	// asserted epic: a second chevron stacked back along the line, the way a
	// quotient is usually drawn (X ↠ Y)
	if (isEpic())
	{
		const QPointF tip2 = tip - dir * (headLength * 1.15);
		painter->drawLine(tip2, tip2 - dir * headLength + normal * headWidth);
		painter->drawLine(tip2, tip2 - dir * headLength - normal * headWidth);
	}

	// asserted monic: a vee at the tail, split back along the line, the way an
	// inclusion is usually drawn (X ↣ Y). Its apex points the way the arrow
	// goes, so the tail reads as a mirror of the head rather than a bar across it.
	if (isMonic())
	{
		const QPointF tail = path.pointAtPercent(0.0);
		QPointF tailDir = path.pointAtPercent(0.04) - tail;
		const qreal tailLen = qSqrt(tailDir.x() * tailDir.x() + tailDir.y() * tailDir.y());
		tailDir /= (tailLen > 1e-6 ? tailLen : 1);
		const QPointF tailNormal(-tailDir.y(), tailDir.x());
		const QPointF apex = tail + tailDir * headLength;
		painter->drawLine(apex, tail + tailNormal * headWidth);
		painter->drawLine(apex, tail - tailNormal * headWidth);
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

void Arrow::setProperties(const QStringList& keys)
{
	qDeleteAll(m_props);
	m_props.clear();
	for (const QString& key : keys)
		addProperty(key);
}

bool Arrow::isMonic() const
{
	return has(Monomorphism::Key());
}

bool Arrow::isEpic() const
{
	return has(Epimorphism::Key());
}

void Arrow::setMonic(bool monic)
{
	if (monic == isMonic())
		return;
	if (monic) addProperty(Monomorphism::Key());
	else removeProperty(Monomorphism::Key());
	update();
	emit styleChanged(this);
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

	// what this arrow is asserted to be, cancellable on the left or the
	// right (or both - though in most categories that still falls short of
	// invertible). Checkable, like Exists such: the ASSERTION, not a
	// construction, so it lives here rather than under Construct.
	Arrow* self = this;
	QAction* monic = menu.addAction(Emoji::monomorphism() + "  Monomorphism");
	monic->setCheckable(true);
	monic->setChecked(isMonic());
	monic->setToolTip(QString("Cancellable on the left: for g, h : Z %1 X, f%2g = f%2h implies g = h. "
	                         "Drawn with a hooked tail, the way an inclusion usually is.")
		.arg(Emoji::to(), Emoji::compose()));
	QObject::connect(monic, &QAction::toggled, &menu, [self](bool on) { self->setMonicRecorded(on); });

	QAction* epic = menu.addAction(Emoji::epimorphism() + "  Epimorphism");
	epic->setCheckable(true);
	epic->setChecked(isEpic());
	epic->setToolTip(QString("Cancellable on the right: for g, h : Y %1 Z, g%2f = h%2f implies g = h. "
	                        "Drawn with a doubled head, the way a quotient usually is.")
		.arg(Emoji::to(), Emoji::compose()));
	QObject::connect(epic, &QAction::toggled, &menu, [self](bool on) { self->setEpicRecorded(on); });
	menu.addSeparator();

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
