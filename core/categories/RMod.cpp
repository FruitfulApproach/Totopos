#include "core/categories/RMod.h"
#include "core/NodeKind.h"
#include "art/RModule.h"

RMod::RMod(QGraphicsItem* parent)
	: Category("R-Mod", parent)
{
	setProperties({ "hasProducts", "hasCoproducts", "hasEqualizers", "hasCoequalizers", "hasZeroObject", "hasKernels", "isAdditive", "isAbelian", "isConcrete", "isLocallySmall" });
}

QString RMod::objectKind() const
{
	return NodeKind::module();
}

Object* RMod::makeObject(const QString& name)
{
	// Not a plain Object: what is placed in R-Mod is a module, and a module
	// knows its zero and how its elements scale.
	return new RModule(name, this);
}
