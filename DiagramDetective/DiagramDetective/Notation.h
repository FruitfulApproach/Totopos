#pragma once

#include <QString>

// Turning what is typed into what is meant. 1_X is written 1 with a subscript
// X; there is no reason to leave the underscore standing in the diagram.
namespace Notation
{
	// 1_X -> the subscript form, 1^{op} -> the superscript form. A single
	// character follows the marker, or a braced group. A group containing
	// anything Unicode has no such form for is left EXACTLY as typed rather
	// than half converted.
	QString withScripts(const QString& text);

	// the subscript / superscript of one character, or a null QChar
	QChar subscriptFor(QChar c);
	QChar superscriptFor(QChar c);
}
