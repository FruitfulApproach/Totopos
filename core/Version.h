#pragma once

#include <QString>

// WHAT VERSION THIS IS, said in one place.
//
// The number is written here and nowhere else: the splash screen, the About
// box and anything that writes a file header all read it from here, so there
// is no second copy to forget. It is not the SAVED FILE's version, which is a
// different number about a different thing and lives in SceneFile.
namespace Version
{
	inline QString number()   { return QStringLiteral("0.9.0"); }
	inline QString name()     { return QStringLiteral("Totopos"); }
	// what is shown to a person: "Totopos 0.9.0"
	inline QString full()     { return name() + QLatin1Char(' ') + number(); }
}
