#include "core/categories/BigCat.h"

#include "core/NodeKind.h"

BigCat::BigCat(QGraphicsItem* parent)
	: Category("BigCat", parent)
{
	setProperties({ "hasProducts", "hasCoproducts", "hasEqualizers", "hasCoequalizers" });
}

QString BigCat::objectKind() const
{
	return NodeKind::category();
}

Object* BigCat::makeObject(const QString& name)
{
	// an object of BigCat is a category: drawn as one, and a place to put
	// objects in - unlike a plain object, which holds nothing
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
