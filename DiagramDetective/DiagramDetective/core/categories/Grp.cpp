#include "core/categories/Grp.h"

Grp::Grp(QGraphicsItem* parent)
	: Category("Grp", parent)
{
	setProperties({ "hasProducts", "hasCoproducts", "hasEqualizers", "hasCoequalizers", "hasZeroObject", "isConcrete", "isLocallySmall" });
}
