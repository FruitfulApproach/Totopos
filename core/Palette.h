#pragma once

#include <QColor>

// THE COLOURS THE PROGRAM DRAWS IN, in one place and under names.
//
// Every one of these was a QColor(r, g, b) written where it was used, and the
// same idea was written differently in different files: an error was
// QColor(255, 0, 0) on an object and QColor(220, 38, 38) on the mark struck
// through it, a highlight was one green here and another there. Naming them
// says which of them are THE SAME COLOUR - and makes a change of scheme one
// edit rather than a search.
//
// The scheme: a warm off-white paper, cobalt for everything drawn ON it,
// and a small set of accents that each mean one thing. They are chosen to
// sit together in a diagram that may show all of them at once, and to stay
// legible where they are laid over one another at low alpha.
namespace Palette
{
	// THE PAPER. Warm rather than white: a pure white sheet with a bright
	// yellow category on it glares, and every printed diagram this program
	// imitates is on paper that is slightly off-white.
	inline QColor paper()        { return QColor(0xF7, 0xF5, 0xEF); }

	// THE INK. Everything drawn on the paper that is not saying anything in
	// particular: an arrow, the frame round an object.
	inline QColor ink()          { return QColor(0x2A, 0x2F, 0x3A); }
	inline QColor cobalt()       { return QColor(0x2F, 0x6F, 0xED); }

	// A REGION, as against a thing standing in one. A category is a field the
	// diagram is drawn in, so it is a wash of colour rather than a solid.
	inline QColor field()        { return QColor(0xF6, 0xC8, 0x5F); }   // warm amber
	// A THING. Objects are drawn in the cooler of the two, so an object
	// inside a category reads as standing ON the field rather than as part
	// of it.
	inline QColor thing()        { return QColor(0x3F, 0xB8, 0xAF); }   // teal

	// THE ACCENTS, one meaning each.
	inline QColor wrong()        { return QColor(0xE5, 0x48, 0x4D); }   // a diagram that cannot mean what it says
	inline QColor pointedAt()    { return QColor(0x30, 0xA4, 0x6C); }   // lit up from somewhere else
	inline QColor picked()       { return QColor(0x7C, 0x3A, 0xED); }   // selected by hand
	inline QColor offered()      { return QColor(0xF5, 0x9E, 0x0B); }   // a handle held out to be pressed
	inline QColor held()         { return QColor(0xB4, 0x53, 0x09); }   // locked, and saying so

	// The same colour at a given alpha, for the washes. Written this way so a
	// call site says WHICH colour and HOW FAINT separately, rather than
	// carrying a fourth number that has to be read as an alpha.
	inline QColor faded(const QColor& colour, int alpha)
	{
		QColor out = colour;
		out.setAlpha(alpha);
		return out;
	}
}
