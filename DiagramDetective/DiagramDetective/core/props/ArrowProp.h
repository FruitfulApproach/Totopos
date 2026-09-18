#pragma once

#include <QStringList>
#include "core/props/Prop.h"

class Arrow;
class QMenu;

// A proposition about an arrow: this functor maps elements, this arrow is
// monic, ... What it lets the user do lives in arrowContextMenu.
class ArrowProp : public Prop
{
	Q_OBJECT

public:
	explicit ArrowProp(Arrow* arrow = nullptr);

	// the arrow this is about (its owner)
	Arrow* arrow() const { return m_arrow; }

	// a section in the arrow's right-click menu, or nothing
	virtual void arrowContextMenu(QMenu& menu, Arrow* arrow) { Q_UNUSED(menu); Q_UNUSED(arrow); }

	// the registry of built-in arrow properties (ArrowProp.cpp)
	static QStringList keys();
	static ArrowProp* create(const QString& key, Arrow* arrow);

private:
	Arrow* m_arrow = nullptr;
};
