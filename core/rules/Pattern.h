#pragma once

#include <QList>
#include <QHash>
#include <QString>
#include <QSet>
#include <QMetaType>

class Node;
class Arrow;
class Category;
class Rule;

// A diagram as plain data: no QGraphicsItem, no pointers, nothing that belongs
// to a thread. Everything the matcher needs to know about a node is copied in,
// and every reference between nodes becomes an index.
//
// This exists so the search can run OFF the GUI thread. Node and Arrow are
// QObjects living in the GUI thread, and a QGraphicsScene is not safe to read
// while that thread may be editing it, so a background search cannot walk the
// live diagram. It walks one of these instead: captured on the GUI thread in a
// single cheap pass, then handed over and never touched again.
//
// Indices are the whole trick. A match comes back as pattern index -> diagram
// index, and the GUI thread turns those back into live nodes with the parallel
// array it kept when it captured (see RuleSearch).
struct PatternNode
{
	QString label;            // Node::id(): what was typed
	QString effectiveLabel;   // Arrow::effectiveId(), or the label for an object
	bool isArrow = false;
	bool isCategory = false;
	int parent = -1;          // the node this one is drawn in; -1 for the root
	int domain = -1;          // an arrow's ends, when both are inside this pattern
	int codomain = -1;
	bool rowsExact = false;
	bool columnsExact = false;
};

class Pattern
{
public:
	// The whole of a diagram, from its ambient category down. Index 0 is the
	// ambient category itself. GUI thread only - it reads live nodes.
	//
	// `liveNodes`, when given, comes back as the parallel array index -> node,
	// which is how a match is read back afterwards.
	static Pattern fromDiagram(Category* ambient, QList<Node*>* liveNodes = nullptr);

	// A rule's premise, in the order the search wants it: index 0 is the
	// rule's root, then premiseObjects() parents-first, then premiseArrows().
	// That order is fixed and reproducible from the file, which is what lets a
	// match be stored as ordinals and resolved against a fresh parse later
	// (see RuleSearch::applyEntry).
	static Pattern fromRulePremise(const Rule& rule);

	const QList<PatternNode>& nodes() const { return m_nodes; }
	int size() const { return int(m_nodes.size()); }
	const PatternNode& at(int index) const { return m_nodes.at(index); }

	// the nodes drawn directly inside `parent`, split the way the search asks
	// for them: an arrow is never an object
	const QList<int>& objectsIn(int parent) const;
	const QList<int>& arrowsIn(int parent) const;

	// Where a rule whose root is called `label` could sit: the ambient
	// category when the name is right, and every category of that name drawn
	// anywhere inside it.
	QList<int> rootsNamed(const QString& label) const;

	// EVERY category in here, the ambient one included: where a rule whose
	// root is a VARIABLE could sit. "For any category C..." is about R-Mod,
	// about Top, about a category drawn inside another one - about all of
	// them, so the search is offered all of them.
	QList<int> everyCategory() const;

	// Is that the name of a built-in (R-Mod, Ab, Set, ...)? A rule root
	// called one of those is about THAT category; a root called anything
	// else - C, D, a letter - stands for any category at all.
	static bool namesABuiltIn(const QString& label);

	// how many premise objects a rule pattern holds (index 1 .. objectCount)
	int objectCount() const { return m_objectCount; }
	int arrowCount() const { return m_arrowCount; }

	// THE UNIVERSE ABOVE THE CANVAS, as a diagram index.
	//
	// A canvas is a category, drawn in nothing - but it sits in a universe
	// one step up, and is an object of the category of categories there. That
	// universe is never drawn, so it has no index of its own among the nodes;
	// this stands for it, and it is what a rule's root is bound to when the
	// rule is read one universe up (see Rule::universeSubject). Nothing is
	// ever looked up by it: it says only that the root is the implied
	// universe rather than anything on the page.
	static constexpr int Universe = -2;

	// The pattern index of the category such a rule is about - its C - or -1
	// when this premise cannot be read that way. Set from the rule itself
	// (see Rule::universeSubject).
	int universeSubject() const { return m_universeSubject; }

private:
	void index();

	QList<PatternNode> m_nodes;
	QHash<int, QList<int>> m_objectChildren;
	QHash<int, QList<int>> m_arrowChildren;
	QHash<QString, QList<int>> m_categoriesByLabel;
	int m_objectCount = 0;
	int m_arrowCount = 0;
	int m_universeSubject = -1;
};

// One place a pattern sits inside a diagram, as indices into each.
struct PatternMatch
{
	QHash<int, int> objects;   // pattern index -> diagram index
	QHash<int, int> arrows;
};

Q_DECLARE_METATYPE(Pattern)

namespace PatternMatcher
{
	// Every place `pattern` sits inside `diagram`, as a subgraph with the same
	// nesting and the same arrows between the same things - constants matching
	// constants, variables standing for anything, consistently, and no two
	// pattern nodes standing for one diagram node.
	//
	// Pure data in, pure data out: safe to call from any thread.
	QList<PatternMatch> find(const Pattern& pattern, const Pattern& diagram, int cap = 200);
}
