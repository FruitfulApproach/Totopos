#pragma once

#include <QString>
#include <QList>

class Node;
class Category;
class DiagramScene;

// What a node IS, as something the properties page can offer and change.
//
// The kinds are told apart by the C++ class the node is an instance of - an
// object, an element, a category, and each built-in category its own class -
// so changing one is not a flag being set: the node is built afresh as the
// other kind and everything that is not about the kind is carried over. Its
// name, where it sits, what is drawn inside it, the arrows that end on it
// and what it asserts all survive the change; only what it can DO differs.
namespace NodeKind
{
	// The id of a kind, as it travels through the combo boxes and the file.
	// A built-in category is "category:R-Mod"; the bare "category" is one the
	// user defined, carrying whatever structure was ticked for it.
	QString object();         // "object"      - an object of the category it is in
	QString module();         // "module"      - an R-module, which has a zero and scales
	QString element();        // "element"     - an element of the node it is in
	QString subcategory();    // "subcategory" - a subcategory of the category it is in
	QString category();       // "category"    - a category of its own
	QString builtIn(const QString& name);   // "category:R-Mod"

	// the built-in name inside a "category:..." id, or empty
	QString builtInOf(const QString& id);
	bool isCategoryKind(const QString& id);

	// what this node is now
	QString of(const Node* node);

	// One entry of the Type list: what it would become, what to call it, and
	// why you would.
	struct Choice
	{
		QString id;
		QString label;
		QString tip;
	};

	// What this node could be turned into WHERE IT SITS. Subcategory is only
	// offered inside a category, because a subcategory of nothing is not a
	// thing; the ambient category is not offered anything, because it is the
	// canvas.
	QList<Choice> choices(const Node* node);
	// the label of a kind on its own, for a header
	QString label(const QString& id);

	// Build a fresh node of that kind, ready to take the place of `like`
	// (which decides where it goes and, for a subcategory, what kind of
	// category it has to be). Never puts it in the scene.
	Node* create(const QString& kindId, const QString& name, Node* like);

	// Move everything that is not about the kind from one shell to the other,
	// put the new one where the old one was, and take the old one out of the
	// scene without destroying it. Both must be nodes of the same diagram.
	void transplant(Node* from, Node* to);

	// Turn this node into that kind, and put the change in the scene's
	// history. Returns the node that has taken its place, or the node itself
	// when nothing had to change.
	Node* retype(Node* node, const QString& kindId);
}
