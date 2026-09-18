#pragma once

#include "art/Object.h"

// An ELEMENT of the object it is drawn in, and nothing more: x in M, not a
// world of its own. Every other object here is a category in its own right -
// double-click into it and you are placing things inside it - and that is
// exactly what an element is not. Nothing goes inside an element, so it is
// the one node that ends the nesting.
//
// Drawn as its name with a small filled dot beside it, so an element can be
// told from an object at a glance without reading the properties page.
class AtomicElement : public Object
{
	Q_OBJECT

public:
	explicit AtomicElement(const QString& id, QGraphicsItem* parent = nullptr);

	// the dot lives to the left of the label, so the frame has to make room
	QRectF boundingRect() const override;
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

	// "Element x of M"
	QString contextTitle() const override;

	// how far to the left of the label the dot sits, and how big it is
	static qreal dotSpan() { return 13.0; }
	static qreal dotRadius() { return 3.0; }
};
