#pragma once

#include "core/props/Prop.h"

// A proposition about a sketch - the diagram drawn inside one category: it
// commutes, its rows are exact, ...
//
// These no longer hang off a sketch object. SketchWithinCategory, which used
// to own them, is gone along with the nested-subobject model it indexed; what
// a diagram is asserted to be now lives on DiagramScene (commutes) and on the
// nodes of each connected component (rowsExactInComponent and friends).
class SketchProp : public Prop
{
	Q_OBJECT

public:
	explicit SketchProp(QObject* parent = nullptr);
};

// The sketch commutes: any two parallel paths in it are equal.
class Commutes : public SketchProp
{
	Q_OBJECT
public:
	explicit Commutes(QObject* parent = nullptr) : SketchProp(parent) {}
	static QString Key() { return QStringLiteral("commutes"); }
	QString key() const override { return Key(); }
	QString label() const override { return QStringLiteral("Commutes"); }
	QString description() const override { return QStringLiteral("Any two parallel paths of arrows in the sketch are equal."); }
};
