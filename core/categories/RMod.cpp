#include "core/categories/RMod.h"
#include "art/RModule.h"

RMod::RMod(QGraphicsItem* parent)
	: Category("R-Mod", parent)
{
	setProperties({ "hasProducts", "hasCoproducts", "hasEqualizers", "hasCoequalizers", "hasZeroObject", "hasKernels", "isAdditive", "isAbelian", "isConcrete", "isLocallySmall" });
}

Object* RMod::makeObject(const QString& name)
{
	// Not a plain Object: what is placed in R-Mod is a module, and a module
	// knows its zero and how its elements scale.
	return new RModule(name, this);
}
