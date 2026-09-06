#include "RMod.h"

RMod::RMod(QGraphicsItem* parent)
	: Category("R-Mod", parent)
{
	setProperties({ "hasProducts", "hasCoproducts", "hasEqualizers", "hasCoequalizers", "hasZeroObject", "hasKernels", "isAdditive", "isAbelian", "isConcrete", "isLocallySmall" });
}
