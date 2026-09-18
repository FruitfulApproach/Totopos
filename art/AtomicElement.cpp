#include "art/AtomicElement.h"
#include "art/Category.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>

AtomicElement::AtomicElement(const QString& id, QGraphicsItem* parent)
	: Object(id, parent)
{
}

QRectF AtomicElement::boundingRect() const
{
	// Object frames the label; the dot is drawn outside that, on the left
	return Object::boundingRect().adjusted(-dotSpan(), 0, 0, 0);
}

void AtomicElement::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	Object::paint(painter, option, widget);

	// The dot is what says this is an element rather than an object, so it is
	// drawn whatever else this node is painted with. It follows the border
	// colour when one has been chosen, and is a plain dark dot otherwise.
	const QRectF frame = boundingRect();
	const QPointF centre(frame.left() + dotSpan() / 2.0, frame.center().y());
	QColor ink = border().style() == Qt::NoPen ? QColor(60, 60, 70) : border().color();
	if (hasError())
		ink = QColor(255, 0, 0);
	if (isHighlighted())
		ink = QColor(22, 163, 74);
	ink.setAlpha(255);

	painter->setPen(Qt::NoPen);
	if (existsSuch())
	{
		// claimed to exist, not given: a ring rather than a filled dot, the
		// same way a dotted border reads on everything else
		painter->setBrush(Qt::NoBrush);
		painter->setPen(QPen(ink, 1.2));
	}
	else
	{
		painter->setBrush(QBrush(ink));
	}
	painter->drawEllipse(centre, dotRadius(), dotRadius());
}

QString AtomicElement::contextTitle() const
{
	// what it is an element OF is the node it is drawn in, not the category
	// that node belongs to: x is an element of M, and M is the R-module
	auto* home = dynamic_cast<Node*>(parentItem());
	const QString name = id().isEmpty() ? QStringLiteral("an element") : id();
	if (home == nullptr || home->id().isEmpty())
		return QString("Element %1").arg(name);
	return QString("Element %1 of %2").arg(name, home->id());
}
