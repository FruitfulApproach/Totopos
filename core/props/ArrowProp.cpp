#include "core/props/ArrowProp.h"
#include "core/props/MapsElements.h"
#include "core/props/ArrowProps.h"
#include "art/Arrow.h"

ArrowProp::ArrowProp(Arrow* arrow)
	: Prop(arrow)   // an Arrow is a QGraphicsObject, hence a QObject: it owns its properties
	, m_arrow(arrow)
{
}

QStringList ArrowProp::keys()
{
	return { MapsElements::Key(), Monomorphism::Key(), Epimorphism::Key() };
}

ArrowProp* ArrowProp::create(const QString& key, Arrow* arrow)
{
	if (key == MapsElements::Key()) return new MapsElements(arrow);
	if (key == Monomorphism::Key()) return new Monomorphism(arrow);
	if (key == Epimorphism::Key()) return new Epimorphism(arrow);
	return nullptr;
}
