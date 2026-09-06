#pragma once

#include "Node.h"

class Object  : public Node
{
	Q_OBJECT

public:
	Object(const QString& id, QGraphicsItem *parent=nullptr);
	~Object() {}

	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
	QRectF boundingRect() const override { return childrenBoundingRect().adjusted(-5, -5, 5, 5); }

	Category* category() const;
};

