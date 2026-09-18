#include "core/categories/Ring.h"

Ring::Ring(QGraphicsItem* parent)
	: Category("Ring", parent)
{
	setProperties({ "hasProducts", "hasCoproducts", "hasEqualizers", "hasCoequalizers", "isConcrete", "isLocallySmall" });
}
