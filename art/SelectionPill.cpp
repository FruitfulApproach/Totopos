#include "art/SelectionPill.h"

#include "core/Palette.h"

#include <QCursor>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace
{
	// The bar, in device pixels: it ignores the view's transform, so these
	// are what is on the screen whatever the zoom.
	const qreal kTall = 26.0;
	const qreal kWide = 34.0;
	const qreal kRound = 8.0;
	const qreal kClear = 14.0;   // how far above the selection the bar sits
}

SelectionPill::SelectionPill(QGraphicsItem* parent)
	: QGraphicsObject(parent)
{
	setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
	setAcceptedMouseButtons(Qt::LeftButton);
	setAcceptHoverEvents(true);
	setCursor(Qt::PointingHandCursor);
	setZValue(1000);   // over the diagram: it is a control, not part of it
}

QList<SelectionPill::Button> SelectionPill::buttons() const
{
	// Buttons are placed symmetrically about x=0 so the pill stays centred
	// over the selection regardless of which buttons are visible.
	if (m_showAddNode)
	{
		// copy left, add-node right, gap of 4 between them
		const qreal gap = 4.0;
		const qreal halfTotal = kWide + gap / 2.0;
		Button copy;
		copy.where = QRectF(-halfTotal, -kTall, kWide, kTall);
		copy.isCopy = true;
		Button add;
		add.where = QRectF(gap / 2.0, -kTall, kWide, kTall);
		add.isCopy = false;
		return { copy, add };
	}
	Button copy;
	copy.where = QRectF(-kWide / 2.0, -kTall, kWide, kTall);
	copy.isCopy = true;
	return { copy };
}

void SelectionPill::setShowAddNode(bool show)
{
	if (m_showAddNode == show)
		return;
	prepareGeometryChange();
	m_showAddNode = show;
	update();
}

int SelectionPill::buttonAt(const QPointF& where) const
{
	const QList<Button> all = buttons();
	for (int at = 0; at < all.size(); ++at)
		if (all.at(at).where.contains(where))
			return at;
	return -1;
}

QRectF SelectionPill::boundingRect() const
{
	QRectF all;
	for (const Button& button : buttons())
		all |= button.where;
	return all.adjusted(-3, -3, 3, 4);
}

void SelectionPill::hoverOver(const QRectF& sceneBounds)
{
	if (sceneBounds.isNull())
		return;
	setPos(sceneBounds.center().x(), sceneBounds.top() - kClear);
}

void SelectionPill::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
	painter->setRenderHint(QPainter::Antialiasing, true);

	const QList<Button> all = buttons();
	for (int at = 0; at < all.size(); ++at)
	{
		const Button& button = all.at(at);
		QColor face = Palette::ink();
		if (at == m_hot)
			face = face.lighter(at == m_pressed ? 105 : 125);

		painter->setPen(QPen(QColor(255, 255, 255, 200), 1.2));
		painter->setBrush(face);
		painter->drawRoundedRect(button.where, kRound, kRound);

		const QPointF middle = button.where.center();
		if (button.isCopy)
		{
			// TWO SHEETS OF PAPER, one behind the other
			const qreal w = 9.0;
			const qreal h = 12.0;
			painter->setPen(QPen(QColor(255, 255, 255, 240), 1.3));
			painter->setBrush(Qt::NoBrush);
			painter->drawRoundedRect(QRectF(middle.x() - w / 2 - 2, middle.y() - h / 2 - 2, w, h), 1.6, 1.6);
			painter->setBrush(QColor(255, 255, 255, 235));
			painter->drawRoundedRect(QRectF(middle.x() - w / 2 + 2, middle.y() - h / 2 + 2, w, h), 1.6, 1.6);
		}
		else
		{
			// A SMALL BOX WITH A PLUS INSIDE: add a node
			const qreal s = 10.0;   // box side
			const qreal arm = 4.5;  // half-length of the cross arms
			painter->setPen(QPen(QColor(255, 255, 255, 235), 1.4));
			painter->setBrush(Qt::NoBrush);
			painter->drawRoundedRect(QRectF(middle.x() - s / 2, middle.y() - s / 2, s, s), 1.8, 1.8);
			painter->drawLine(QPointF(middle.x(), middle.y() - arm),
			                  QPointF(middle.x(), middle.y() + arm));
			painter->drawLine(QPointF(middle.x() - arm, middle.y()),
			                  QPointF(middle.x() + arm, middle.y()));
		}
	}
}

void SelectionPill::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
	m_pressed = buttonAt(event->pos());
	update();
	event->accept();   // ours: nothing under it is selected or dragged
}

void SelectionPill::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
	const int pressed = m_pressed;
	m_pressed = -1;
	update();
	event->accept();

	// only when let go of on the button it was pressed on, which is what a
	// button means
	if (pressed < 0 || buttonAt(event->pos()) != pressed)
		return;
	const QList<Button> all = buttons();
	if (pressed < all.size() && all.at(pressed).isCopy)
		emit copyAsked();
	else
		emit addNodeAsked();
}

void SelectionPill::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
	const int was = buttonAt(event->pos());
	if (was != m_hot)
	{
		m_hot = was;
		if (m_hot < 0)
		{
			setToolTip(QString());
		}
		else
		{
			const QList<Button> all = buttons();
			setToolTip(m_hot < all.size() && all.at(m_hot).isCopy
				? QStringLiteral("Copy these and carry them. Click where they should "
				                 "go - in this diagram or another - and they are put "
				                 "down there.")
				: QStringLiteral("Add a new object inside the selected node."));
		}
		update();
	}
	QGraphicsObject::hoverMoveEvent(event);
}

void SelectionPill::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
	m_hot = -1;
	m_pressed = -1;
	update();
	QGraphicsObject::hoverLeaveEvent(event);
}
