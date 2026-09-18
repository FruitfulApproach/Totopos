#include "art/ArrowHandle.h"
#include "art/Node.h"

#include <QPainter>
#include <QCursor>

namespace
{
	// device pixels: the same size at any zoom. Big, because it is only up for
	// a couple of seconds and has to be arrived at in that time.
	const qreal kRadius = 13.0;
	const qreal kGap = 3.0;   // clear air between one button and the next
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

void ArrowHandle::showFor(Node* from, const QList<Button>& buttons, const QPointF& scenePos)
{
	if (from == nullptr || buttons.isEmpty())
	{
		hideHandle();
		return;
	}
	// Already up, offering the same thing, and not the sort that follows the
	// mouse: leave it exactly where it was put. Moving it here is what made
	// the + and - impossible to press - they slid away from under the cursor
	// on the way to them.
	const bool same = isVisible() && m_node == from && m_buttons == buttons;
	if (same && staysPut())
		return;

	prepareGeometryChange();   // a different number of buttons is a different rect
	m_node = from;
	m_buttons = buttons;
	setPos(scenePos);
	show();
}

void ArrowHandle::hideHandle()
{
	m_node = nullptr;
	m_buttons.clear();
	hide();
}

QPointF ArrowHandle::centreOf(int index) const
{
	// the row is centred on the cursor, so the first button sits left of it
	// when there is more than one
	const qreal step = 2 * kRadius + kGap;
	const qreal offset = (index - (m_buttons.size() - 1) / 2.0) * step;
	return QPointF(offset, 0);
}

bool ArrowHandle::buttonAt(const QPointF& scenePos, Button& which) const
{
	const QPointF at = mapFromScene(scenePos);
	for (int i = 0; i < m_buttons.size(); ++i)
	{
		const QPointF d = at - centreOf(i);
		if (d.x() * d.x() + d.y() * d.y() <= kRadius * kRadius * 1.6)
		{
			which = m_buttons.at(i);
			return true;
		}
	}
	// The press was not squarely on a circle. For a row that HOLDS STILL
	// that is an answer in itself: the cursor is somewhere else entirely and
	// the press belongs to whatever is under it, not to a button a few
	// pixels away. Only the lone button that rides the cursor takes a near
	// miss, where there is nothing else the press could have been meant for.
	if (m_buttons.isEmpty() || staysPut())
		return false;
	int best = 0;
	qreal bestD = -1;
	for (int i = 0; i < m_buttons.size(); ++i)
	{
		const QPointF d = at - centreOf(i);
		const qreal n = d.x() * d.x() + d.y() * d.y();
		if (bestD < 0 || n < bestD) { bestD = n; best = i; }
	}
	which = m_buttons.at(best);
	return true;
}

QRectF ArrowHandle::boundingRect() const
{
	QRectF r;
	for (int i = 0; i < m_buttons.size(); ++i)
		r |= QRectF(centreOf(i) - QPointF(kRadius, kRadius), QSizeF(2 * kRadius, 2 * kRadius));
	return r.adjusted(-2, -2, 2, 2);
}

QPainterPath ArrowHandle::shape() const
{
	QPainterPath path;
	for (int i = 0; i < m_buttons.size(); ++i)
		path.addEllipse(centreOf(i), kRadius, kRadius);
	return path;
}

void ArrowHandle::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
	painter->setRenderHint(QPainter::Antialiasing, true);

	for (int i = 0; i < m_buttons.size(); ++i)
	{
		const QPointF c = centreOf(i);
		// Orange, and not a colour anything in a diagram is drawn in: this is
		// a control that has appeared over the work, and it should not be
		// mistaken for part of it even for a moment.
		painter->setPen(QPen(QColor(255, 255, 255, 235), 1.6));
		painter->setBrush(QColor(245, 158, 11, 245));
		painter->drawEllipse(c, kRadius, kRadius);

		switch (m_buttons.at(i))
		{
		case Button::DrawArrow:
		{
			// a triangle pointing the way an arrow goes
			QPolygonF head;
			head << c + QPointF(6.4, 0) << c + QPointF(-4.1, 6.0) << c + QPointF(-4.1, -6.0);
			painter->setPen(Qt::NoPen);
			painter->setBrush(Qt::white);
			painter->drawPolygon(head);
			break;
		}
		}
	}
}

bool ArrowHandle::coversScenePos(const QPointF& scenePos, qreal slack) const
{
	if (!isVisible() || m_buttons.isEmpty())
		return false;
	const QPointF at = mapFromScene(scenePos);
	const qreal r = kRadius + slack;
	for (int i = 0; i < m_buttons.size(); ++i)
	{
		const QPointF d = at - centreOf(i);
		if (d.x() * d.x() + d.y() * d.y() <= r * r)
			return true;
	}
	return false;
}
