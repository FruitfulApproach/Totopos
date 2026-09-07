#pragma once

#include "Node.h"

class Object  : public Node
{
	Q_OBJECT

public:
	Object(const QString& id, QGraphicsItem *parent=nullptr);
	~Object() {}

	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
	QRectF boundingRect() const override { return childFrame().adjusted(-2.5, -2.5, 2.5, 2.5); }

	// What can be clicked. An object with nothing inside it paints NOTHING -
	// no fill, no border, just its label - but it is still the object, so its
	// whole frame takes the mouse. Painting and hit-testing are separate
	// questions and this one must not follow the other.
	QPainterPath shape() const override;

	Category* category() const;

	// "R-module M", "Set X", "Category C". Public, as on Node: the dock asks
	// for it through a Category pointer, and an override in a protected
	// section would hide it there.
	QString contextTitle() const override;

protected:
	// the press that begins either gesture; the scene decides which
	void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
};

