#include "Object.h"

#include <QStyleOptionGraphicsItem>

Object::Object(const QString& id, QGraphicsItem *parent)
	: Node(id, parent)
{
	// flags belong here, not in paint(): paint() runs every frame and must not mutate state
	setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsFocusable
	         | QGraphicsItem::ItemSendsGeometryChanges);
}

void Object::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	Node::paint(painter, option, widget);

	painter->setBrush(fill());
	painter->setPen(border());
	painter->drawRoundedRect(boundingRect(), 5, 5);

	if (option->state & QStyle::State_Selected)
	{
		// a selection outline, since the border itself may be invisible
		QPen sel(QColor(99, 102, 241), 1, Qt::DashLine);
		painter->setBrush(Qt::NoBrush);
		painter->setPen(sel);
		painter->drawRoundedRect(boundingRect(), 5, 5);
	}
}

