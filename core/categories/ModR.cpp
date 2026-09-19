#include "core/categories/ModR.h"
#include "art/RModule.h"

ModR::ModR(QGraphicsItem* parent)
	: Category("Mod-R", parent)
{
	setProperties({ "hasProducts", "hasCoproducts", "hasEqualizers", "hasCoequalizers", "hasZeroObject", "hasKernels", "isAdditive", "isAbelian", "isConcrete", "isLocallySmall" });
}

Object* ModR::makeObject(const QString& name)
{
	// Not a plain Object: what is placed in Mod-R is a module, and a module
	// knows its zero and how its elements scale.
	return new RModule(name, this);
}
