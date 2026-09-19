#include "core/categories/BuiltInCategories.h"

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

namespace
{
	// The letters of a name, in order, with everything a person varies freely
	// taken out: case, spaces, hyphens, underscores and full stops. R-Mod,
	// R-mod, RMod and "R Mod" all come out "rmod".
	QString bones(const QString& name)
	{
		QString out;
		for (QChar c : name)
			if (c.isLetterOrNumber())
				out += c.toLower();
		return out;
	}
}

QString Category::builtInNamed(const QString& label)
{
	const QString want = bones(label);
	if (want.isEmpty())
		return QString();
	for (const QString& name : builtInNames())
		if (bones(name) == want)
			return name;
	return QString();
}
