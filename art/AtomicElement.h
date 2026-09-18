#pragma once

#include "art/Object.h"

// An ELEMENT of the object it is drawn in, and nothing more: x in M, not a
// world of its own. Every other object here is a category in its own right -
// double-click into it and you are placing things inside it - and that is
// exactly what an element is not. Nothing goes inside an element, so it is
// the one node that ends the nesting.
//
// Drawn as nothing but its name, centred like any other node. What tells it
// from an object is not a mark of its own but where it IS - inside an object
// rather than beside one - and what it offers when pointed at: + and - rather
// than an arrow.
class AtomicElement : public Object
{
	Q_OBJECT

public:
	explicit AtomicElement(const QString& id, QGraphicsItem* parent = nullptr);

	// "Element x of M"
	QString contextTitle() const override;

	// an arrow joins two OBJECTS; x in M is not one of them
	bool canStartArrow() const override { return false; }

protected:
	// WHAT CAN BE DONE WITH AN ELEMENT, on the right button.
	//
	// These used to be little round buttons on the hover handle, which put
	// them under the cursor at the cost of a row of icons standing over the
	// diagram and no room for a third. They live here instead, beside
	// everything else a node offers: x + y and x - y where the module is
	// additive, and x = y wherever there are elements at all.
	void populateActions(QMenu& menu) override;
};
