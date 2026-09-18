#include "art/ArrowHandle.h"
#include "art/Node.h"

#include <QPainter>
#include <QCursor>

namespace
{
	// device pixels: the same size at any zoom
	const qreal kRadius = 8.5;
}

ArrowHandle::ArrowHandle(QGraphicsItem* parent)
	: QGraphicsObject(parent)
{
	setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
	setZValue(10002);   // over the handle bar, over everything
	// It lies on the cursor, so it must never be what a click lands on nor
	// what hit-testing finds: the scene reads it directly instead.
	setAcceptedMouseButtons(Qt::NoButton);
	setAcceptHoverEvents(false);
	setCursor(QCursor(Qt::PointingHandCursor));
	hide();
}

void ArrowHandle::showFor(Node* from, const QPointF& scenePos)
{
	if (from == nullptr)
	{
		hideHandle();
		return;
	}
	m_node = from;
	setPos(scenePos);
	show();
}

void ArrowHandle::hideHandle()
{
	m_node = nullptr;
	hide();
}

QRectF ArrowHandle::boundingRect() const
{
	return QRectF(-kRadius - 2, -kRadius - 2, 2 * kRadius + 4, 2 * kRadius + 4);
}

QPainterPath ArrowHandle::shape() const
{
	QPainterPath path;
	path.addEllipse(QPointF(0, 0), kRadius, kRadius);
	return path;
}

void ArrowHandle::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
	painter->setRenderHint(QPainter::Antialiasing, true);
	painter->setPen(QPen(QColor(255, 255, 255, 230), 1.3));
	painter->setBrush(QColor(99, 102, 241, 240));
	painter->drawEllipse(QPointF(0, 0), kRadius, kRadius);

	// a triangle pointing the way an arrow goes
	QPolygonF head;
	head << QPointF(4.2, 0) << QPointF(-2.7, 3.9) << QPointF(-2.7, -3.9);
	painter->setPen(Qt::NoPen);
	painter->setBrush(Qt::white);
	painter->drawPolygon(head);
}
