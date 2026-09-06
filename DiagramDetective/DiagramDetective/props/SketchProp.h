#pragma once

#include "Prop.h"

class SketchWithinCategory;

// A proposition about a sketch: it commutes, its rows are exact, ...
class SketchProp : public Prop
{
	Q_OBJECT

public:
	explicit SketchProp(SketchWithinCategory* sketch = nullptr);

	// the sketch this is about (its owner)
	SketchWithinCategory* sketch() const { return m_sketch; }

private:
	SketchWithinCategory* m_sketch = nullptr;
};

// The sketch commutes: any two parallel paths in it are equal.
class Commutes : public SketchProp
{
	Q_OBJECT
public:
	explicit Commutes(SketchWithinCategory* sketch = nullptr) : SketchProp(sketch) {}
	static QString Key() { return QStringLiteral("commutes"); }
	QString key() const override { return Key(); }
	QString label() const override { return QStringLiteral("Commutes"); }
	QString description() const override { return QStringLiteral("Any two parallel paths of arrows in the sketch are equal."); }
};
