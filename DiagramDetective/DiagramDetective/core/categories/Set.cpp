#include "core/categories/Set.h"

Set::Set(QGraphicsItem* parent)
	: Category("Set", parent)
{
	setProperties({ "hasProducts", "hasCoproducts", "hasEqualizers", "hasCoequalizers", "isConcrete", "isLocallySmall" });
}
