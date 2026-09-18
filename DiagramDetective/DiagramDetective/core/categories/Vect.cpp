#include "core/categories/Vect.h"

Vect::Vect(QGraphicsItem* parent)
	: Category("Vect", parent)
{
	setProperties({ "hasProducts", "hasCoproducts", "hasEqualizers", "hasCoequalizers", "hasZeroObject", "hasKernels", "isAdditive", "isAbelian", "isConcrete", "isLocallySmall" });
}
