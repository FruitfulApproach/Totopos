#pragma once

#include <QString>

class DiagramScene;

// The library on disk: DiagramDetective/library/<path>/<name>.totopos, each
// file one definition, axiom, theorem, conjecture, remark or proof, with the
// steps that made it kept inside.
namespace Library
{
	// where the library lives, found by looking beside the program and then
	// up towards the source tree; empty when there is none
	QString root();
	// make it if it is not there
	QString ensureRoot();

	// Write the examples that ship with the program, if they are not already
	// there. Built by the same code that reads them back, rather than by hand:
	// the format is binary, and a file written by guesswork is a file that
	// cannot be opened.
	int seedExamples(QString* error = nullptr);

	// the snake lemma, drawn and chased, saved at
	// library/hom-alg/lemma/snake-lemma.totopos
	bool writeSnakeLemma(const QString& path, QString* error = nullptr);
}
