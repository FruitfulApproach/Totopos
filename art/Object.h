#pragma once

#include "art/Node.h"

class Object  : public Node
{
	Q_OBJECT

public:
	Object(const QString& id, QGraphicsItem *parent=nullptr);
	~Object() {}

	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

	// The box round what this object holds, with a little air (see
	// Node::boxRect) - and squared off when the object is a single letter.
	QRectF boxRect() const override;
	// the box together with the label, which may have been dragged outside it
	QRectF boundingRect() const override { return boxRect() | childFrame(); }

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
	// An object no longer holds anything (see Category::makeObject), so its
	// "Add object" puts the new one BESIDE it, in the category they both
	// belong to. Category overrides this again and puts one inside itself.
	void populateActions(QMenu& menu) override;

	// the press that carries this object about
	void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
};

