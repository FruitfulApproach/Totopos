#include "ArrowProp.h"
#include "MapsElements.h"
#include "../Arrow.h"

ArrowProp::ArrowProp(Arrow* arrow)
	: Prop(arrow)   // an Arrow is a QGraphicsObject, hence a QObject: it owns its properties
	, m_arrow(arrow)
{
}

QStringList ArrowProp::keys()
{
	return { MapsElements::Key() };
}

ArrowProp* ArrowProp::create(const QString& key, Arrow* arrow)
{
	if (key == MapsElements::Key()) return new MapsElements(arrow);
	return nullptr;
}
