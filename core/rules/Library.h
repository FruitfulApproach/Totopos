#pragma once

#include <QString>

class DiagramScene;

// The library on disk: Totopos/library/<path>/<name>.totopos, each
// file one definition, axiom, theorem, conjecture, remark or proof, with the
// steps that made it kept inside.
namespace Library
{
	// where the library lives, found by looking beside the program and then
	// up towards the source tree; empty when there is none
	QString root();
	// make it if it is not there
	QString ensureRoot();

	// A FILE AS IT IS REFERRED TO, not as it happens to sit on this machine.
	//
	// "Grp/classic/inverses exist.axiom.totopos" - from the library folder
	// down, in forward slashes. That is the name a rule goes by everywhere
	// else in the program: what a proof records as the rule it cited, what
	// the file a proof proves is named by, and what is worth putting in a
	// window title or on the clipboard. An absolute path is no use for any
	// of those and too long for all of them.
	//
	// A file outside the library - or one opened before there is a library -
	// has no such name, and comes back as its own path in native separators.
	QString relativePath(const QString& path);

	// Write the examples that ship with the program, if they are not already
	// there. Built by the same code that reads them back, rather than by hand:
	// the format is binary, and a file written by guesswork is a file that
	// cannot be opened.
	int seedExamples(QString* error = nullptr);

	// The standard rules, a folder per category (library/StandardRules.cpp).
	// Anything already on disk is left exactly as it is, so a rule that has
	// been edited stays edited.
	int writeStandardRules(QString* error = nullptr);

	// the snake lemma, drawn and chased, saved at
	// library/R-Mod/snake-lemma.theorem.totopos
	bool writeSnakeLemma(const QString& path, QString* error = nullptr);
}
