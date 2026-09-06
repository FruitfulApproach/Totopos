#include "Cat.h"

Cat::Cat(QGraphicsItem* parent)
	: Category("Cat", parent)
{
	setProperties({ "hasProducts", "hasCoproducts", "hasEqualizers", "hasCoequalizers", "isLocallySmall" });
}

Object* Cat::makeObject(const QString& name)
{
	auto* small = new Category(name, this);
	small->setProperties({ "isLocallySmall" });
	return small;
}
