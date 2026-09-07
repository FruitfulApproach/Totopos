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

Arrow* BigCat::createArrow(const QString& name, Node* from, Node* to)
{
	// between two categories: a functor, which knows how to map their contents
	auto* dom = dynamic_cast<Category*>(from);
	auto* cod = dynamic_cast<Category*>(to);
	if (dom != nullptr && cod != nullptr)
	{
		auto* functor = new Functor(name, dom, cod, this);
		functor->setZValue(2);
		functor->refreshDepthAppearance();
		functor->refreshFrame();
		return functor;
	}
	return Category::createArrow(name, from, to);
}
