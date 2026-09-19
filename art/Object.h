#pragma once

#include "art/Node.h"

class AtomicElement;

class Object  : public Node
{
	Q_OBJECT

public:
	Object(const QString& id, QGraphicsItem *parent=nullptr);
	~Object() override;

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

	// CAN THINGS BE NAMED INSIDE THIS ONE?
	//
	// True when this object belongs to a category whose objects are sets: an
	// R-module has elements, a set has elements, a category drawn in BigCat
	// does not. An element itself never does - it is where the nesting stops.
	bool holdsElements() const;

	// Put an element in this object, at a scene position, named after the ones
	// already there: x, y, z, then x', y', ...
	AtomicElement* createElement(const QPointF& scenePos);
	// the same, named for you - what a functor's image of an element is called
	AtomicElement* createElement(const QString& name, const QPointF& scenePos);

	// CAN SOMETHING BE DRAWN INSIDE THIS, AND WHAT WOULD IT BE?
	//
	// A functor draws the image of what is in its domain into its codomain,
	// and has no business knowing whether that means an object of a category
	// or an element of a module - it asks for a child by name and gets
	// whichever a child of this node IS.
	virtual bool canHoldNamedChildren() const { return holdsElements(); }
	virtual Object* createNamedChild(const QString& name, const QPointF& scenePos);
	// the name the next one would take
	QString nextElementName() const;
	// An element of ours has just been named by hand: x renamed to s makes
	// the next element t. Same rule as a category's objects.
	void noteElementNamed(const QString& name);

	// The "Add element" entry, placing into THIS object at that scene point.
	// Public because an element offers it for its own parent.
	void addElementAction(QMenu& menu, const QPointF& atScene);

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

private:
	// where to START looking for a free element name, not a tally of how
	// many there have been. Not saved: on reopening a file the scan from x
	// finds the same gaps anyway.
	int m_nextElementIndex = 0;
};

