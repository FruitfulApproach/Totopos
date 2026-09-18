#include "core/categories/Top.h"

Top::Top(QGraphicsItem* parent)
	: Category("Top", parent)
{
	setProperties({ "hasProducts", "hasCoproducts", "hasEqualizers", "hasCoequalizers", "isConcrete", "isLocallySmall" });
}
