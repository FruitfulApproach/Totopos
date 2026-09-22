#include "core/categories/Cat.h"
#include "core/NodeKind.h"

Cat::Cat(QGraphicsItem* parent)
	: Category("Cat", parent)
{
	setProperties({ "hasProducts", "hasCoproducts", "hasEqualizers", "hasCoequalizers", "isLocallySmall" });
}

QString Cat::objectKind() const
{
	return NodeKind::category();
}

Object* Cat::makeObject(const QString& name)
{
	auto* small = new Category(name, this);
	small->setProperties({ "isLocallySmall" });
	return small;
}

Arrow* Cat::createArrow(const QString& name, Node* from, Node* to)
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
