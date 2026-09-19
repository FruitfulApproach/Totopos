#pragma once

#include "art/Object.h"

// AN ABSTRACT LEFT R-MODULE, drawn as an object of R-Mod.
//
// An object of R-Mod was a plain Object: a box with a name, which held
// elements because R-Mod is concrete and said nothing else about them. But an
// R-module is not a bag of elements - it is a bag of elements you can ADD, and
// MULTIPLY BY A SCALAR, with a zero that is there whether or not anybody drew
// it. Those are the three things this class knows, and they are the three
// things that were missing.
//
// It is still an Object and still drawn as one: what tells it apart is what it
// offers - a zero, a scalar multiple of what is drawn inside it - and what it
// calls itself when asked.
//
// LEFT, not right: r . x, the scalar on the left, which is what R-Mod means.
// Mod-R makes the same kind of object and writes its scalars on the right (see
// scalarOnLeft), because that is the whole of the difference between them as
// far as anything drawn here is concerned.
class RModule : public Object
{
	Q_OBJECT

public:
	explicit RModule(const QString& id, QGraphicsItem* parent = nullptr);

	// "R-module M", and "R-module M over S" when the ring has been said to be
	// something other than the R that the category is named for.
	QString contextTitle() const override;

	// THE RING IT IS A MODULE OVER, by name.
	//
	// "R" unless told otherwise, because the category is called R-Mod and the
	// R in both is the same R. Kept per module rather than per category so a
	// diagram can carry an R-module and a Z-module without needing two
	// categories to hold them, which is how such a diagram is usually drawn.
	QString ring() const { return m_ring; }
	void setRing(const QString& ring);

	// Which side the scalars are written on: r.x here, x.r in Mod-R. Read off
	// the category this module is drawn in, so a module moved from one to the
	// other writes itself the other way round without being told.
	bool scalarOnLeft() const;

	// r.x, as an element of this module, drawn beside the x it is made from.
	// The label is BUILT from x rather than copied, so renaming x renames this
	// too - the same as the way x + y follows both its parts.
	AtomicElement* createScalarMultiple(const QString& scalar, AtomicElement* of);

	// The zero of this module: an element named 0, and only ever one of them.
	// Returns the one already drawn if there is one, so "add the zero" twice
	// does not give a module with two zeros in it.
	AtomicElement* zeroElement(const QPointF& scenePos);
	AtomicElement* existingZero() const;

protected:
	// what a module offers over and above what any object does: its zero, and
	// somewhere to say what ring it is over
	void populateActions(QMenu& menu) override;

private:
	QString m_ring = QStringLiteral("R");
};
