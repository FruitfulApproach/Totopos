#pragma once

#include <QString>

class DiagramScene;

// Reading and writing a diagram. The format is binary and compressed: a
// diagram is a tree of nodes with a few numbers each, and the history of what
// was done to it - the steps only, never where a node was dragged to.
namespace SceneFile
{
	QString filter();      // for the file dialogs
	QString extension();   // "totopos"

	// What a file says it is, without reading the diagram: the kind and the
	// name live in the clear at the front, so a library of hundreds can be
	// listed without opening any of them.
	struct Heading
	{
		bool valid = false;
		int kind = 0;
		QString name;
	};
	Heading peek(const QString& path);

	bool save(DiagramScene* scene, const QString& path, QString* error = nullptr);
	bool load(DiagramScene* scene, const QString& path, QString* error = nullptr);
}
