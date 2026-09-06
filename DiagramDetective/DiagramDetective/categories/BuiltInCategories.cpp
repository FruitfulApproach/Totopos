#include "BuiltInCategories.h"

QStringList Category::builtInNames()
{
	// BigCat first: the default ambient category
	return { "BigCat", "Cat", "Set", "Ab", "R-Mod", "Mod-R", "Grp", "Ring", "Top", "Vect" };
}

Category* Category::createBuiltIn(const QString& name, QGraphicsItem* parent)
{
	if (name == "BigCat") return new BigCat(parent);
	if (name == "Cat") return new Cat(parent);
	if (name == "Set") return new Set(parent);
	if (name == "Ab") return new Ab(parent);
	if (name == "R-Mod") return new RMod(parent);
	if (name == "Mod-R") return new ModR(parent);
	if (name == "Grp") return new Grp(parent);
	if (name == "Ring") return new Ring(parent);
	if (name == "Top") return new Top(parent);
	if (name == "Vect") return new Vect(parent);
	return nullptr;   // not built in: the caller makes a plain Category
}
