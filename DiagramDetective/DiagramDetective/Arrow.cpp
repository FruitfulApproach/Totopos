#include "Arrow.h"

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

bool Arrow::segment(QPointF& from, QPointF& to) const
{
	if (m_domain == nullptr || m_codomain == nullptr)
		return false;
	const QRectF rd = mapFromItem(m_domain, m_domain->boundingRect()).boundingRect();
	const QRectF rc = mapFromItem(m_codomain, m_codomain->boundingRect()).boundingRect();
	QPointF dir = rc.center() - rd.center();
	const qreal len = qSqrt(dir.x() * dir.x() + dir.y() * dir.y());
	if (len < 1e-6)
		return false;
	dir /= len;
	from = exitPoint(rd, dir) + dir * 3;
	to = exitPoint(rc, -dir) - dir * 3;
	return true;
}

void Arrow::refreshGeometry()
{
	prepareGeometryChange();
	ancestorsPrepareGeometryChange();
	QPointF a, b;
	if (segment(a, b) && label() != nullptr)
	{
		// the label rides the midpoint, offset to the upper side of the arrow
		QPointF dir = b - a;
		const qreal len = qSqrt(dir.x() * dir.x() + dir.y() * dir.y());
		dir /= (len > 1e-6 ? len : 1);
		QPointF normal(-dir.y(), dir.x());
		if (normal.y() > 0)
			normal = -normal;
		const QRectF lr = label()->boundingRect();
		const QPointF mid = (a + b) / 2 + normal * 12;
		label()->setPos(mid.x() - lr.width() / 2, mid.y() - lr.height() / 2);
	}
	update();
	ancestorsUpdate();
}

QRectF Arrow::boundingRect() const
{
	QPointF a, b;
	QRectF r = segment(a, b) ? QRectF(a, b).normalized() : QRectF(-2, -2, 4, 4);
	return r.adjusted(-14, -14, 14, 14) | childrenBoundingRect();
}

QPainterPath Arrow::shape() const
{
	QPointF a, b;
	QPainterPath path;
	if (!segment(a, b))
		return path;
	path.moveTo(a);
	path.lineTo(b);
	QPainterPathStroker stroker;
	stroker.setWidth(10);   // easy to hit, hard to hit by accident
	return stroker.createStroke(path);
}

void Arrow::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	Node::paint(painter, option, widget);
	QPointF a, b;
	if (!segment(a, b))
		return;

	QPen pen = border();
	if (pen.style() == Qt::NoPen)
		pen = QPen(Qt::black, 1.5);
	if (option->state & QStyle::State_Selected)
		pen.setWidthF(pen.widthF() + 1.5);
	painter->setPen(pen);
	painter->setBrush(Qt::NoBrush);
	painter->drawLine(a, b);

	// the head: a filled triangle at the codomain end
	QPointF dir = b - a;
	const qreal len = qSqrt(dir.x() * dir.x() + dir.y() * dir.y());
	dir /= (len > 1e-6 ? len : 1);
	const QPointF normal(-dir.y(), dir.x());
	QPolygonF head;
	head << b << b - dir * 11 + normal * 5 << b - dir * 11 - normal * 5;
	painter->setPen(Qt::NoPen);
	painter->setBrush(pen.color());
	painter->drawPolygon(head);
}
