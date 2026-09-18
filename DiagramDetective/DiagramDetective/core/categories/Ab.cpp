#include "core/categories/Ab.h"

Ab::Ab(QGraphicsItem* parent)
	: Category("Ab", parent)
{
	setProperties({ "hasProducts", "hasCoproducts", "hasEqualizers", "hasCoequalizers", "hasZeroObject", "hasKernels", "isAdditive", "isAbelian", "isConcrete", "isLocallySmall" });
}
