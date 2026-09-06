#include "BigCat.h"

BigCat::BigCat(QGraphicsItem* parent)
	: Category("BigCat", parent)
{
	setProperties({ "hasProducts", "hasCoproducts", "hasEqualizers", "hasCoequalizers" });
}

Object* BigCat::makeObject(const QString& name)
{
	// an object of BigCat is a category: drawn as one, and itself a place to double-click into
	return new Category(name, this);
}
