#include "SketchProp.h"
#include "../SketchWithinCategory.h"

SketchProp::SketchProp(SketchWithinCategory* sketch)
	: Prop(sketch)   // the sketch owns its properties
	, m_sketch(sketch)
{
}
