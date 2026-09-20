#include "art/Object.h"
#include "core/Palette.h"
#include "art/Category.h"
#include "art/AtomicElement.h"
#include "core/Emoji.h"
#include <QMenu>
#include <QAction>
#include "art/DiagramScene.h"
#include <QGraphicsSceneMouseEvent>

#include <QStyleOptionGraphicsItem>
#include <QPainterPath>
#include "core/AppSettings.h"
#include "art/GraphicsHelpers.h"
#include <QDebug>

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
	setDefaultLook(QBrush(Palette::faded(Palette::thing(), 105)), QPen(Palette::cobalt(), 1.6));

	// AND WHAT WAS ASKED FOR, which wins over the look above.
	//
	// "Set default" on the Properties page stores the colours the next one
	// placed should start in. Written through setFill/setBorder, so it DOES
	// count as chosen: somebody picked these, and a node picked out that way
	// shows its frame whether or not it holds anything, exactly as one
	// coloured by hand does.
	const QColor wantedFill = AppSettings::instance().defaultFill(false);
	const QColor wantedBorder = AppSettings::instance().defaultBorder(false);
	if (wantedFill.isValid())
		setFill(QBrush(wantedFill));
	if (wantedBorder.isValid())
		setBorder(QPen(wantedBorder, border().widthF() > 0 ? border().widthF() : 1.6));

	// and the colour its name is written in, if one was asked for
	if (const QColor wantedText = AppSettings::instance().defaultText(false); wantedText.isValid())
		setLabelColour(wantedText);

	// the label is the handle you move it by, and says so
	if (NodeLabel* text = labelItem())
		text->setCursor(Qt::SizeAllCursor);
}

Object::~Object()
{
	safeRemoveAndLog(this, "Object");
}

QRectF Object::boxRect() const
{
	// A node that HOLDS something is a frame round what it holds, and wants
	// air between the frame and its contents. A node that holds nothing is
	// its label and nothing else - a module called M is the letter M - so it
	// gets barely any: the border, where one is drawn at all, sits close in.
	// (contentFrame gives the letters themselves for a node that holds
	// nothing, so this is air round the GLYPH, not round its text line.)
	const qreal air = containedCount() == 0 ? 2.5 : 1.5;
	const QRectF box = contentFrame().adjusted(-air, -air, air, air);

	// one letter is drawn in a square, and the square is centred on it
	return squareIfSingleGlyph(box);
}

void Object::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	Node::paint(painter, option, widget);

	QPen pen = border();
	QBrush brush = fill();
	// every mark this function invents is written for an outermost node and
	// taken down a step for each node this one sits inside
	const qreal scale = depthScale();

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
			pen = QPen(Palette::ink(), 1.6 * scale);
		pen.setStyle(Qt::DotLine);
	}
	if (hasError())
	{
		// part of something the diagram cannot mean: shown, not hidden
		const Qt::PenStyle style = pen.style() == Qt::DotLine ? Qt::DotLine : Qt::SolidLine;
		pen = QPen(Palette::wrong(), 2.5 * scale);
		pen.setStyle(style);
	}
	if (isHighlighted())
	{
		// last word: whatever this object is painted with, right now it is
		// being pointed at
		pen = QPen(Palette::pointedAt(), 3.0 * scale);
		brush = QBrush(Palette::faded(Palette::pointedAt(), 70));
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
			pen.setWidthF(pen.widthF() + 1.6 * scale);
	}

	painter->setBrush(brush);
	painter->setPen(pen);
	// the BOX, not boundingRect(): that one also covers the label, which may
	// have been dragged clear of the frame (see Node::boxRect)
	painter->drawRoundedRect(boxRect(), drawnCornerRadius(), drawnCornerRadius());

	if (selected && !existsSuch())
	{
		painter->setBrush(Qt::NoBrush);
		painter->setPen(QPen(Palette::picked(), 1.0 * scale, Qt::DashLine));
		painter->drawRoundedRect(boxRect(), drawnCornerRadius(), drawnCornerRadius());
	}

	// Struck off: this one goes when the rule is applied. Drawn last, over
	// everything else, because it is not one of the object's looks - it is a
	// mark made ON it.
	if (markedForDeletion())
	{
		const QRectF frame = boxRect();
		const qreal reach = qMin(qreal(11.0) * scale, qMin(frame.width(), frame.height()) / 2);
		const QPointF at = frame.center();
		painter->setBrush(Qt::NoBrush);
		painter->setPen(QPen(Palette::wrong(), 2.5 * scale, Qt::SolidLine, Qt::RoundCap));
		painter->drawLine(at + QPointF(-reach, -reach), at + QPointF(reach, reach));
		painter->drawLine(at + QPointF(-reach, reach), at + QPointF(reach, -reach));
	}
}


bool Object::holdsElements() const
{
	// nothing goes inside an element: it is the one node that ends the nesting
	if (dynamic_cast<const AtomicElement*>(this) != nullptr)
		return false;
	Category* home = surroundingCategory();
	return home != nullptr && home->objectsAreSets();
}

QString Object::nextElementName() const
{
	// x, y, z, then a, b, ..., w, then round again with a prime. Skips
	// whatever is already in here, so deleting y and adding one gives y back
	// rather than a fourth name nobody asked for.
	for (int at = m_nextElementIndex; at < m_nextElementIndex + 400; ++at)
	{
		const QString name = Category::letterName(at, QLatin1Char('x'));
		bool taken = false;
		for (QGraphicsItem* child : childItems())
			if (auto* node = dynamic_cast<Node*>(child); node != nullptr && node->id() == name)
			{
				taken = true;
				break;
			}
		if (!taken)
			return name;
	}
	return Category::letterName(m_nextElementIndex, QLatin1Char('x'));
}

void Object::noteElementNamed(const QString& name)
{
	const int at = Category::variableIndex(name, QLatin1Char('x'));
	if (at >= 0)
		m_nextElementIndex = at;   // from there, not after it: see Category::noteNamed
}

Object* Object::createNamedChild(const QString& name, const QPointF& scenePos)
{
	return canHoldNamedChildren() ? createElement(name, scenePos) : nullptr;
}

AtomicElement* Object::createElement(const QPointF& scenePos)
{
	return createElement(nextElementName(), scenePos);
}

AtomicElement* Object::createElement(const QString& name, const QPointF& scenePos)
{
	prepareGeometryChange();   // our frame is the union of what we hold
	auto* made = new AtomicElement(name, this);
	made->setPos(mapFromScene(scenePos));
	made->setZValue(1);
	made->refreshDepthAppearance();
	made->refreshFrame();   // it is whole now: our frame can grow to hold it
	return made;
}

void Object::addElementAction(QMenu& menu, const QPointF& atScene)
{
	auto* diagram = diagramOf(this);
	Category* home = surroundingCategory();
	if (diagram == nullptr || home == nullptr)
		return;

	QAction* add = menu.addAction(QString("%1  Add element").arg(Emoji::add()));
	add->setToolTip(QString("Name an element of %1. The objects of %2 have underlying sets, so an "
	                        "element of one is something that can be drawn and carried across an "
	                        "arrow.").arg(id(), home->id()));
	Object* self = this;
	// the nearest clear grid point to where the menu was opened: an element
	// asked for while the cursor is on one already must not land on it
	const QPointF clear = mapToScene(freeGridSpotIn(this, mapFromScene(atScene)));
	QObject::connect(add, &QAction::triggered, diagram, [diagram, self, atScene = clear] {
		// queued: the menu is still closing, and this puts a node in the scene
		QMetaObject::invokeMethod(diagram, [diagram, self, atScene] {
			if (AtomicElement* made = self->createElement(atScene))
				diagram->recordCreation(
					QString("Put %1 in %2").arg(made->id(), self->id()), { made });
		}, Qt::QueuedConnection);
	});
	menu.addSeparator();
}

void Object::populateActions(QMenu& menu)
{
	// An object of a concrete category has an underlying SET, so the thing to
	// put in it is an ELEMENT, drawn inside it. Elsewhere - an object of
	// BigCat, say - there is no such thing, and what the menu can offer is
	// another object beside this one.
	if (holdsElements())
	{
		addElementAction(menu, mapToScene(contextPos()));
		return;
	}

	// An element holds nothing itself, but what it IS one of can hold more:
	// the entry offers a sibling element, put where the menu was opened.
	if (dynamic_cast<AtomicElement*>(this) != nullptr)
	{
		if (auto* owner = dynamic_cast<Object*>(parentItem());
		    owner != nullptr && owner->holdsElements())
			owner->addElementAction(menu, mapToScene(contextPos()));
		return;
	}

	addObjectAction(menu, surroundingCategory());
}

QPainterPath Object::shape() const
{
	QPainterPath path;
	path.addRoundedRect(boxRect(), drawnCornerRadius(), drawnCornerRadius());
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
