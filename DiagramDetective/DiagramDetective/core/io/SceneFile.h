#pragma once

#include <QString>
#include <QByteArray>
#include <QList>
#include <QPointF>

class DiagramScene;
class Category;
class Node;

// Reading and writing a diagram. The format is binary and compressed: a
// diagram is a tree of nodes with a few numbers each, and the history of what
// was done to it - the steps only, never where a node was dragged to.
namespace SceneFile
{
	QString filter();      // for the file dialogs
	QString extension();   // "totopos"

	// WHAT A FILE IS, WRITTEN IN ITS NAME.
	//
	// Every diagram can be read as a rule: solid is the pattern, dotted is what
	// the rule says there then is, and what is crossed out in red is what it
	// takes away. What KIND of claim that amounts to is a different question,
	// and the answer is in the file name:
	//
	//     <name>.<kind>.totopos      kernel.definition.totopos
	//                                snake-lemma.theorem.totopos
	//                                snake-lemma.proof.totopos
	//
	// so a library of hundreds can be listed, and its axioms told from its
	// conjectures, without opening a single file. A name with no kind in it is
	// a free drawing: something drawn, not yet put forward as anything.
	//
	// "theorem", "axiom", ... for a kind; empty for Unstated (a free drawing is
	// written plainly, without ".free-draw." in its name)
	QString kindInfix(int kind);
	// the kind written into that file's name, or Unstated when there is none
	int kindFromFileName(const QString& path);
	// "kernel" + Definition -> "kernel.definition.totopos"
	QString fileNameFor(const QString& baseName, int kind);
	// the name without the kind or the extension: "kernel.definition.totopos" -> "kernel"
	QString baseNameOf(const QString& path);

	// What a file says it is, without reading the diagram. The kind comes from
	// the name; the name of the statement lives in the clear at the front of
	// the file.
	struct Heading
	{
		bool valid = false;
		int kind = 0;
		QString name;
	};
	Heading peek(const QString& path);

	bool save(DiagramScene* scene, const QString& path, QString* error = nullptr);
	bool load(DiagramScene* scene, const QString& path, QString* error = nullptr);

	// ---------------------------------------------------------------- fragments
	//
	// A piece of a diagram, for the clipboard and for dragging between scenes.
	// The same records as a whole file, but the arrows name their ends BY THEIR
	// PLACE IN THE FRAGMENT rather than by a path from the ambient category -
	// the fragment is going somewhere else, where that path means nothing.

	// the MIME type a fragment travels under
	QString fragmentMimeType();

	// Everything in `nodes` and everything inside them. A node whose ancestor
	// is also in the list is left out (it comes with its ancestor), and an
	// arrow is carried only when BOTH its ends are in the fragment.
	QByteArray copyFragment(const QList<Node*>& nodes);

	// Draw it into `into`, with what was the top-left of the fragment landing
	// at `atScenePos`. Names already taken get a prime. What was made is
	// returned; `dropped` is how many arrows had an end that did not come.
	QList<Node*> pasteFragment(const QByteArray& payload, Category* into,
	                           const QPointF& atScenePos, int* dropped = nullptr);
}
