#pragma once

#include <QHash>
#include <QPointF>
#include <QPointer>
#include <QString>

class Category;
class DiagramScene;
class Implies;
class Node;

// THE SAME STATEMENT, WRITTEN THE LONG WAY.
//
// A diagram here says a whole implication in one picture: what is drawn solid
// is what you are given, what is drawn dashed is what there then IS, and what
// is crossed out in red is what goes. That is the succinct notation, and it is
// how the diagram is stored, matched, saved and reasoned about. Nothing in
// this file changes any of that.
//
// The classical notation says the same thing the way it is said on paper:
//
//         [ X -f-> Y -g-> Z ]  ==R==>  [ the commuting triangle, gf and all ]
//               the givens                        the conclusion
//
// two diagrams and an implication between them, with the rule's name over the
// arrow. This class BUILDS that picture out of the diagram, and takes it down
// again. It is a view and nothing more:
//
//   * The boxes hold COPIES. The diagram itself is untouched; while the
//     classical view is up the real one is merely hidden.
//   * The copies hang off the scene directly, not off the ambient category,
//     so everything that walks the diagram - saving it, reading it as a rule,
//     searching it for a pattern, listing its components - walks straight past
//     them and cannot see them at all. That is the whole safety argument, and
//     it is why the boxes are not simply put inside the canvas.
//   * The copies are drawn SOLID. The dashes and the red crosses said which
//     half of the implication a thing belonged to; in this notation the two
//     halves are two boxes, so the marks have nothing left to say.
//
// WHAT IS REMEMBERED. A picture of two boxes wants a different arrangement
// from one picture, so the positions given to things in the classical view are
// kept apart from the positions they have in the succinct one. Switch over,
// drag things until the layout reads, switch back, switch over again: both
// arrangements are still there. The succinct positions are the nodes' own and
// are never written by this class; the classical ones are harvested into the
// scene's map when the view comes down, and are saved with the file.
//
// Each copy is stamped with the key of the node it was copied FROM (Node::key,
// which is identity and survives relabelling), so a position outlives a
// rebuild, a rename, and a reopening of the file.
class ClassicalView
{
public:
	// what a copy carries, so a position can be found again
	enum DataKey
	{
		SourceKey = 10,   // Node::key() of the node this is a copy of
		SideKey = 11,     // "given" or "then"
	};

	explicit ClassicalView(DiagramScene* scene);
	// takes the view down; does NOT harvest - the scene does that first
	~ClassicalView();

	// Build the two boxes and the implication between them, putting every copy
	// where `remembered` says it went last time and laying the rest out afresh.
	// False when there is no diagram to read.
	bool build(const QHash<QString, QPointF>& remembered);

	// Where everything in the view has been dragged to, keyed the way build()
	// expects to be given it back. Adds to whatever is already in `into`, so a
	// position set in an earlier session and not touched in this one survives.
	void harvestPositions(QHash<QString, QPointF>& into) const;

	// the two halves, for anything that needs to ask
	Category* givens() const { return m_givens.data(); }
	Category* conclusion() const { return m_conclusion.data(); }
	// is this node part of the view rather than part of the diagram?
	static bool isPartOfView(const Node* node);

	// the rule's name changed: rewrite the label on the arrow
	void refreshName();

private:
	DiagramScene* m_scene = nullptr;
	QPointer<Category> m_givens;
	QPointer<Category> m_conclusion;
	QPointer<Implies> m_implies;
};
