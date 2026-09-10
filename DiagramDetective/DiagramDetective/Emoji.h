#pragma once

#include <QIcon>
#include <QString>
#include <QFont>

// Emoji as glyphs and as icons. Nothing is loaded from disk: the characters
// are drawn with the system emoji font, so there are no image files to keep in
// step with the program.
namespace Emoji
{
	// the detective the program is named after
	QString detective();
	// a font that has the emoji in it, at that pixel size
	QFont font(int pixelSize);

	// any emoji, rendered into an icon at the sizes a window and a taskbar ask for
	QIcon icon(const QString& glyph);
	// the same, cached, for the window and the taskbar
	QIcon appIcon();

	// the ones used about the program, named so the call sites read as words
	inline QString newFile()     { return QStringLiteral("\U0001F4C4"); }   // page
	inline QString openFile()    { return QStringLiteral("\U0001F4C2"); }   // open folder
	inline QString saveFile()    { return QStringLiteral("\U0001F4BE"); }   // floppy disk
	inline QString undo()        { return QStringLiteral("↩"); }       // leftwards arrow with hook
	inline QString redo()        { return QStringLiteral("↪"); }       // rightwards arrow with hook
	inline QString properties()  { return QStringLiteral("\U0001F9FE"); }   // receipt
	inline QString equations()   { return QStringLiteral("\U0001F7F0"); }   // heavy equals sign
	inline QString settings()    { return QStringLiteral("⚙️"); } // gear
	inline QString remove()      { return QStringLiteral("\U0001F5D1️"); } // wastebasket
	inline QString appearance()  { return QStringLiteral("\U0001F3A8"); }   // artist palette
	inline QString shape()       { return QStringLiteral("〰️"); } // wavy dash
	inline QString library()     { return QStringLiteral("\U0001F4DA"); }   // books
	inline QString teach()       { return QStringLiteral("\U0001F9ED"); }   // compass
	inline QString centre()      { return QStringLiteral("🎯"); }   // direct hit
	inline QString fit()         { return QStringLiteral("🔍"); }   // magnifier
	// and the mathematics, which is written in mathematics
	inline QString existsSuch()  { return QStringLiteral("∃"); }       // there exists
	inline QString mapsTo()      { return QStringLiteral("↦"); }       // maps to
	inline QString compose()     { return QStringLiteral("∘"); }       // ring operator
	inline QString to()          { return QStringLiteral("→"); }       // rightwards arrow
	inline QString monomorphism(){ return QStringLiteral("↣"); }       // rightwards arrow with tail
	inline QString epimorphism() { return QStringLiteral("↠"); }       // rightwards two-headed arrow
}
