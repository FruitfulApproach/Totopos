#include "Functor.h"
#include "props/MapsElements.h"

#include <QtGlobal>

Functor::Functor(const QString& name, Category* dom, Category* cod, QGraphicsItem* parent)
	: Arrow(name, dom, cod, parent)   // Arrow has no default constructor: the base is built here
{
	setProperties({ MapsElements::Key() });   // live by default: see MapsElements

	// a functor's ends must sit in the SAME surrounding category
	if (dom != nullptr && cod != nullptr && dom->surroundingCategory() != cod->surroundingCategory())
		qCritical("Functor '%s': domain and codomain must belong to the same surrounding category.", qPrintable(name));
}

Functor::~Functor()
{
}
