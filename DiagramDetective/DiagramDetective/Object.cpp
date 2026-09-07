#include "Object.h"
#include "Category.h"
#include "DiagramScene.h"
#include <QGraphicsSceneMouseEvent>

#include <QStyleOptionGraphicsItem>
#include <QPainterPath>
#include "AppSettings.h"

Object::Object(const QString& id, QGraphicsItem *parent)
	: Node(id, parent)
{
	// flags belong here, not in paint(): paint() runs every frame and must not mutate state
	// NOT ItemIsMovable: dragging an object draws an arrow from it, and moving
	// it is a press and hold. Qt's own dragging would take the first of those
	// away, so the scene does both by hand.
	setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsFocusable
	         | QGraphicsItem::ItemSendsGeometryChanges);
}

void Object::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	Node::paint(painter, option, widget);

	QPen pen = border();
	QBrush brush = fill();

	// An object with nothing drawn inside it is just its label - an R-module
	// is the letter M, not a box with M in it. The frame is what says there is
	// something in here, so it appears the moment anything is placed inside.
	// A colour chosen by hand always shows, and the setting turns the whole
	// rule off for anyone who wants every object boxed.
	if (containedCount() == 0 && !hasChosenStyle() && !AppSettings::instance().frameEmptyNodes())
	{
		pen = QPen(Qt::NoPen);
		brush = QBrush(Qt::NoBrush);
	}

	if (existsSuch())
	{
		// dots around the border: this object is asserted to exist
		if (pen.style() == Qt::NoPen)
			pen = QPen(QColor(60, 60, 70), 1.6);
		pen.setStyle(Qt::DotLine);
	}
	if (hasError())
	{
		// part of something the diagram cannot mean: shown, not hidden
		const Qt::PenStyle style = pen.style() == Qt::DotLine ? Qt::DotLine : Qt::SolidLine;
		pen = QPen(QColor(255, 0, 0), 2.5);
		pen.setStyle(style);
	}
	if (isHighlighted())
	{
		// last word: whatever this object is painted with, right now it is
		// being pointed at
		pen = QPen(QColor(22, 163, 74), 3.0);
		brush = QBrush(QColor(34, 197, 94, 70));
	}

	painter->setBrush(brush);
	painter->setPen(pen);
	painter->drawRoundedRect(boundingRect(), cornerRadius(), cornerRadius());

	if (option->state & QStyle::State_Selected)
	{
		// a selection outline, since the border itself may be invisible
		QPen sel(QColor(99, 102, 241), 1, Qt::DashLine);
		painter->setBrush(Qt::NoBrush);
		painter->setPen(sel);
		painter->drawRoundedRect(boundingRect(), cornerRadius(), cornerRadius());
	}
}


QPainterPath Object::shape() const
{
	QPainterPath path;
	path.addRoundedRect(boundingRect(), cornerRadius(), cornerRadius());
	return path;
}

QString Object::contextTitle() const
{
	Category* home = surroundingCategory();
	if (home == nullptr)
		return Node::contextTitle();   // the canvas itself: just its name
	return QString("%1 %2").arg(sentenceCase(home->objectName()), id());
}

void Object::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		if (auto* diagram = dynamic_cast<DiagramScene*>(scene()))
		{
			// Taken here so this object becomes the one the mouse is on, and
			// the scene keeps getting the moves. Which gesture it turns into -
			// an arrow, or carrying it about - is decided by what happens next.
			diagram->beginPress(this, event->scenePos());
			if (!isSelected())
			{
				scene()->clearSelection();
				setSelected(true);
			}
			event->accept();
			return;
		}
	}
	Node::mousePressEvent(event);
}
