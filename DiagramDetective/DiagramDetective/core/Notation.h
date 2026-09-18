#pragma once

#include <QString>

// Turning what is typed into what is meant. 1_X is written 1 with a subscript
// X; there is no reason to leave the underscore standing in the diagram.
//
// The underscore stays in the SOURCE, though. A label holds exactly what was
// typed - v_{x}, F^{op} - and that raw text is the node's id: it is what gets
// saved, matched against a rule, and edited when the label is opened again.
// The scripts are a matter of DRAWING, applied by toHtml() at the moment the
// text reaches a rich-text document, and never folded back into the source.
//
// (This used to substitute Unicode subscripts instead. Unicode has subscripts
// for ten digits, five signs and seventeen lowercase letters, no capitals at
// all, and no font is obliged to carry the ones it does have - so half the
// names a person could reasonably write came out either unconverted or as
// boxes. <sub> has none of those limits.)
namespace Notation
{
	// v_{x} -> v<sub>x</sub>, F^{op} -> F<sup>op</sup>. A single character
	// follows the marker, or a braced group; a marker at the very end, or an
	// opening brace that is never closed, is left standing as typed.
	//
	// Everything outside the markers is HTML-escaped, so the result can go
	// straight into a document: hand it text with an & or a < in it and the
	// character comes out, not a mangled tag.
	QString toHtml(const QString& text);

	// Whether toHtml() would do anything here. Text without a marker can be
	// set as plain text, which is cheaper and leaves the document's own
	// formatting alone.
	bool hasScripts(const QString& text);
}
