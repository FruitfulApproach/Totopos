#include "art/NodeHandles.h"
#include "art/Arrow.h"
#include "core/AppSettings.h"
#include "core/Emoji.h"

#include <QPainter>
#include <QGraphicsScene>
#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>

namespace
{
	// device pixels: the bar is the same size at any zoom
	const qreal kRadius = 10.0;
	const qreal kGap = 6.0;       // clear air between the node and the first button
	const qreal kSpacing = 6.0;   // between buttons
}

NodeHandles::NodeHandles(QGraphicsItem* parent)
	: QGraphicsObject(parent)
{
	setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
	setZValue(10000);
	setAcceptHoverEvents(true);
	setCursor(QCursor(Qt::PointingHandCursor));
	hide();
}

NodeHandles::~NodeHandles()
{
	if (QGraphicsScene* board = scene())
		board->removeItem(this);
}

void NodeHandles::attach(Node* node, const QPointF& itemPos)
{
	detach();
	m_node = node;
	if (node == nullptr)
		return;
	m_at = itemPos;
	if (buttons().isEmpty())
	{
		m_node = nullptr;   // nothing to offer here
		return;
	}
	connect(node, &Node::moved, this, [this] { place(); });
	connect(node, &Node::deleted, this, [this] { detach(); });
	place();
	show();
}

void NodeHandles::detach()
{
	if (!m_node.isNull())
		disconnect(m_node.data(), nullptr, this, nullptr);
	m_node = nullptr;
	m_hover = -1;
	hide();
}

QList<NodeHandles::Button> NodeHandles::buttons() const
{
	// Only the runner is left, and only where there is something to run with.
	// An object gets no bar at all.
	QList<Button> shown;
	auto* arrow = dynamic_cast<Arrow*>(m_node.data());
	if (arrow != nullptr && arrow->domain() != nullptr && arrow->domain()->holdsAnything())
		shown << Chase;
	return shown;
}

void NodeHandles::place()
{
	if (m_node.isNull())
		return;
	prepareGeometryChange();

	// An arrow: beside the point of the line nearest the click, on the side
	// AWAY from its label, and well clear of the line's own hit band. The
	// label is what a bar too close would crowd, so it is what decides.
	if (auto* arrow = dynamic_cast<Arrow*>(m_node.data()))
	{
		const QPainterPath path = arrow->curve();
		if (!path.isEmpty())
		{
			qreal bestPercent = 0.5;
			qreal bestDistance = -1;
			for (int i = 0; i <= 64; ++i)
			{
				const qreal percent = i / 64.0;
				const QPointF d = path.pointAtPercent(percent) - m_at;
				const qreal distance = d.x() * d.x() + d.y() * d.y();
				if (bestDistance < 0 || distance < bestDistance)
				{
					bestDistance = distance;
					bestPercent = percent;
				}
			}
			const QPointF at = path.pointAtPercent(bestPercent);
			QPointF along = path.pointAtPercent(qMin(1.0, bestPercent + 0.02))
			              - path.pointAtPercent(qMax(0.0, bestPercent - 0.02));
			const qreal length = qSqrt(along.x() * along.x() + along.y() * along.y());
			QPointF normal = length > 1e-6 ? QPointF(-along.y() / length, along.x() / length) : QPointF(0, 1);

			// the label sits on one side of the line; take the other
			const QRectF label = arrow->labelRect();
			if (!label.isEmpty() && QPointF::dotProduct(label.center() - at, normal) > 0)
				normal = -normal;
			m_out = normal;

			// clear of the hit band, and then some: the lift is scene units,
			// the buttons device pixels, so it clears at any zoom
			const qreal lift = AppSettings::instance().arrowHitWidth() / 2.0 + 16.0;
			setPos(m_node->mapToScene(at) + m_out * lift);
			return;
		}
	}

	// an object: under the middle of its bottom edge, the same place every time
	m_out = QPointF(0, 1);
	const QRectF r = m_node->boxRect();   // hang off the frame, not off its label
	setPos(m_node->mapToScene(QPointF(r.center().x(), r.bottom())));
}

QPointF NodeHandles::centreOf(int index) const
{
	// out past the node first, then along it: the whole bar clears the frame
	const QPointF along(-m_out.y(), m_out.x());
	const qreal step = 2 * kRadius + kSpacing;
	const qreal offset = (index - (buttons().size() - 1) / 2.0) * step;
	return m_out * (kGap + kRadius) + along * offset;
}

QRectF NodeHandles::boundingRect() const
{
	QRectF r;
	const int count = buttons().size();
	for (int i = 0; i < count; ++i)
		r |= QRectF(centreOf(i) - QPointF(kRadius, kRadius), QSizeF(2 * kRadius, 2 * kRadius));
	return r.adjusted(-2, -2, 2, 2);
}

QPainterPath NodeHandles::shape() const
{
	QPainterPath path;
	const int count = buttons().size();
	for (int i = 0; i < count; ++i)
		path.addEllipse(centreOf(i), kRadius, kRadius);
	return path;
}

int NodeHandles::indexAt(const QPointF& itemPos) const
{
	const int count = buttons().size();
	for (int i = 0; i < count; ++i)
	{
		const QPointF d = itemPos - centreOf(i);
		if (d.x() * d.x() + d.y() * d.y() <= kRadius * kRadius)
			return i;
	}
	return -1;
}

void NodeHandles::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
	painter->setRenderHint(QPainter::Antialiasing, true);
	const QList<Button> shown = buttons();
	const QPointF along(-m_out.y(), m_out.x());

	for (int i = 0; i < shown.size(); ++i)
	{
		const QPointF c = centreOf(i);
		const bool hot = m_hover == i;
		switch (shown.at(i))
		{
		case Chase:
		{
			// the runner: chase the elements of the domain across
			painter->setPen(QPen(QColor(255, 255, 255, 220), 1.2));
			painter->setBrush(hot ? QColor(21, 128, 61) : QColor(34, 197, 94, 235));
			painter->drawEllipse(c, kRadius, kRadius);
			painter->setPen(Qt::black);
			painter->setFont(Emoji::font(int(kRadius * 1.4)));
			painter->drawText(QRectF(c - QPointF(kRadius, kRadius), QSizeF(2 * kRadius, 2 * kRadius)),
			                  Qt::AlignCenter, QStringLiteral("\U0001F3C3\U0001F3FE‍➡️"));
			break;
		}
		}
	}
}

void NodeHandles::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
	const int index = indexAt(event->pos());
	if (event->button() != Qt::LeftButton || index < 0 || m_node.isNull())
	{
		QGraphicsObject::mousePressEvent(event);
		return;
	}
	Node* node = m_node.data();
	switch (buttons().at(index))
	{
	case Chase: emit chaseRequested(node); break;
	}
	event->accept();
}

void NodeHandles::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
	event->accept();   // swallowed: the first click already did the work
}

void NodeHandles::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
	const int was = m_hover;
	m_hover = indexAt(event->pos());
	if (m_hover == was)
		return;
	QString tip;
	if (m_hover >= 0)
	{
		switch (buttons().at(m_hover))
		{
		case Chase: tip = "Chase: carry the elements of the domain across, and keep them carried"; break;
		}
	}
	setToolTip(tip);
	update();
}

void NodeHandles::hoverLeaveEvent(QGraphicsSceneHoverEvent*)
{
	m_hover = -1;
	update();
}
