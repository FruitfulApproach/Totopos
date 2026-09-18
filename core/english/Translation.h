#pragma once

#include <QString>
#include <QList>

class DiagramScene;
class Node;

// The diagram said out loud.
//
// A picture here is a statement: what is drawn solid is quantified over, what
// is dotted is claimed to exist, what is crossed out in red is taken away. All
// of that can be read as a sentence, and this is where that is done - in
// English, with the mathematics written as mathematics (A -> B, g o f, and
// so on) rather than spelled out in words.
//
// The scene is not asked to change in any way: this only reads.
namespace Translation
{
	// The whole diagram, as rich text: a paragraph or two of prose.
	QString describe(const DiagramScene* scene);

	// Only these nodes (and, for an arrow among them, its two ends, so that
	// "f : A -> B" still has an A and a B to name). An empty list means the
	// whole diagram.
	QString describe(const DiagramScene* scene, const QList<Node*>& only);

	// the same reading with the markup taken off, for the clipboard
	QString asPlainText(const QString& richText);
}
