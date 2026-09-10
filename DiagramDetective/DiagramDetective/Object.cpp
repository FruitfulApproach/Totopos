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
	// NOT ItemIsMovable: dragging an object by its body draws an arrow from
	// it, and dragging it by its LABEL moves it. Qt's own dragging would take
	// the first of those away, so the scene does both by hand.
	setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsFocusable
	         | QGraphicsItem::ItemSendsGeometryChanges);
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
	if (containedCount() == 0 && !alwaysFramed() && !hasChosenStyle() && !AppSettings::instance().frameEmptyNodes())
	{
		pen = QPen(Qt::NoPen);
		brush = QBrush(Qt::NoBrush);
	}

	if (existsSuch() && pen.style() == Qt::NoPen)
	{
		// this object is asserted to exist, so it has a border to dash even
		// when nothing else would have given it one
		pen = QPen(QColor(60, 60, 70), 1.6);
	}
	if (hasError())
	{
		// part of something the diagram cannot mean: shown, not hidden
		pen = QPen(QColor(255, 0, 0), 2.5);
	}
	if (isHighlighted())
	{
		// last word: whatever this object is painted with, right now it is
		// being pointed at
		pen = QPen(QColor(22, 163, 74), 3.0);
		brush = QBrush(QColor(34, 197, 94, 70));
	}
	// Dashes round the border: this object is asserted to exist. Applied last,
	// so the claim still reads on a border an error or a highlight has taken
	// over the colour and the width of.
	if (existsSuch())
		applyExistsDash(pen);

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
			diagram->beginPress(this, event->scenePos(), DiagramScene::Gesture::Arrow);
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
