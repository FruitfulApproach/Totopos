#include "art/Object.h"
#include "art/Category.h"
#include "art/DiagramScene.h"
#include <QGraphicsSceneMouseEvent>

#include <QStyleOptionGraphicsItem>
#include <QPainterPath>
#include "core/AppSettings.h"

Object::Object(const QString& id, QGraphicsItem *parent)
	: Node(id, parent)
{
	// flags belong here, not in paint(): paint() runs every frame and must not mutate state
	// NOT ItemIsMovable: the scene carries objects by hand, so that a drag can
	// also put the node on the grid, shove the neighbours it runs into, and
	// go in the history as one move. Qt's own dragging does none of that.
	setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsFocusable
	         | QGraphicsItem::ItemSendsGeometryChanges);
	// Limegreen in dodger blue, the default look of an object. Set straight on
	// the members rather than through setFill/setBorder, so this does NOT count
	// as a style chosen by hand - an object with nothing in it is still just
	// its label unless the setting says otherwise (see paint).
	setDefaultLook(QBrush(QColor(50, 205, 50, 110)), QPen(QColor(30, 144, 255), 1.6));

	// the label is the handle you move it by, and says so
	if (NodeLabel* text = labelItem())
		text->setCursor(Qt::SizeAllCursor);
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

	// PICKED OUT. The fill comes forward either way, so the object reads as
	// chosen without anything being drawn over it. The border says so in
	// whichever way it is already drawn: an object asserted to exist has a
	// dotted border of its own and merely THICKENS, because what the diagram
	// claims must not be lost to what is only selected. An ordinary object
	// has no such border, so it gets a thin dashed ring instead.
	const bool selected = (option->state & QStyle::State_Selected) != 0;
	if (selected)
	{
		if (brush.style() != Qt::NoBrush)
		{
			QColor c = brush.color();
			c.setAlpha(qMin(255, c.alpha() + 70));
			brush = QBrush(c);
		}
		if (existsSuch() && pen.style() != Qt::NoPen)
			pen.setWidthF(pen.widthF() + 1.6);
	}

	painter->setBrush(brush);
	painter->setPen(pen);
	// the BOX, not boundingRect(): that one also covers the label, which may
	// have been dragged clear of the frame (see Node::boxRect)
	painter->drawRoundedRect(boxRect(), cornerRadius(), cornerRadius());

	if (selected && !existsSuch())
	{
		painter->setBrush(Qt::NoBrush);
		painter->setPen(QPen(QColor(99, 102, 241), 1.0, Qt::DashLine));
		painter->drawRoundedRect(boxRect(), cornerRadius(), cornerRadius());
	}

	// Struck off: this one goes when the rule is applied. Drawn last, over
	// everything else, because it is not one of the object's looks - it is a
	// mark made ON it.
	if (markedForDeletion())
	{
		const QRectF frame = boxRect();
		const qreal reach = qMin(qreal(11.0), qMin(frame.width(), frame.height()) / 2);
		const QPointF at = frame.center();
		painter->setBrush(Qt::NoBrush);
		painter->setPen(QPen(QColor(220, 38, 38), 2.5, Qt::SolidLine, Qt::RoundCap));
		painter->drawLine(at + QPointF(-reach, -reach), at + QPointF(reach, reach));
		painter->drawLine(at + QPointF(-reach, reach), at + QPointF(reach, -reach));
	}
}


void Object::populateActions(QMenu& menu)
{
	addObjectAction(menu, surroundingCategory());
}

QPainterPath Object::shape() const
{
	QPainterPath path;
	path.addRoundedRect(boxRect(), cornerRadius(), cornerRadius());
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
			// Taken here so this object becomes the one the mouse is on and
			// the scene keeps getting the moves. Pressing and dragging a node
			// ALWAYS carries it: an arrow is begun by double-clicking, not by
			// pulling one out of the body.
			diagram->beginPress(this, event->scenePos(), DiagramScene::Gesture::Move);
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
