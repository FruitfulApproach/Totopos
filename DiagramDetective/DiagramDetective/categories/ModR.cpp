#include "ModR.h"

ModR::ModR(QGraphicsItem* parent)
	: Category("Mod-R", parent)
{
	setProperties({ "hasProducts", "hasCoproducts", "hasEqualizers", "hasCoequalizers", "hasZeroObject", "hasKernels", "isAdditive", "isAbelian", "isConcrete", "isLocallySmall" });
}
