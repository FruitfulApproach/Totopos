#include "CategoryProp.h"
#include "../Category.h"

CategoryProp::CategoryProp(Category* category)
	: Prop(category)   // a QGraphicsObject is a QObject: the category owns its properties
	, m_category(category)
{
}
