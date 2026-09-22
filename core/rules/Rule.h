#pragma once

#include <QString>
#include <QList>
#include <QHash>
#include <QRectF>
#include <memory>

class DiagramScene;
class Category;
class Node;
class Arrow;

// A diagram from the library read as a RULE. What is drawn solid is the
// premise - a pattern, every label in it a variable - and what is drawn
// dotted is the conclusion: what the rule says there is, once the pattern
// has been found. "Every R-module holds a 0" is R-Mod holding a solid N
// holding a dotted 0.
class Rule
{
public:
	// read it from a .totopos file; isValid() says whether there was one
	explicit Rule(const QString& path);
	~Rule();

	bool isValid() const { return m_scene != nullptr; }
	QString name() const { return m_name; }
	QString path() const { return m_path; }
	DiagramScene* scene() const { return m_scene.get(); }

	// the pattern: its root (the rule's own ambient category), then the solid
	// objects parents first, then the solid arrows
	Category* root() const { return m_root; }
	const QList<Node*>& premiseObjects() const { return m_premiseObjects; }
	const QList<Arrow*>& premiseArrows() const { return m_premiseArrows; }

	// the conclusion: the dotted objects parents first, and the dotted arrows
	const QList<Node*>& conclusionObjects() const { return m_conclusionObjects; }
	const QList<Arrow*>& conclusionArrows() const { return m_conclusionArrows; }

	// What is crossed out in red. These are part of the PREMISE - they still
	// have to be found - and they are what the rule TAKES AWAY where it is
	// applied. "For all such scenes there is one with these gone and these
	// dotted ones drawn in, and it commutes still."
	const QList<Node*>& deletedObjects() const { return m_deletedObjects; }
	const QList<Arrow*>& deletedArrows() const { return m_deletedArrows; }
	bool deletes() const { return !m_deletedObjects.isEmpty() || !m_deletedArrows.isEmpty(); }

	bool hasConclusion() const { return !m_conclusionObjects.isEmpty() || !m_conclusionArrows.isEmpty(); }

	// A rule that neither adds nor takes away: it only says that this scene is
	// THERE. Applied, it establishes the context rather than changing it -
	// which is exactly what citing an axiom or a theorem amounts to.
	bool isRecogniser() const { return !hasConclusion() && !deletes(); }

	// A label that means one particular thing, and only that thing: 0, 1.
	// Everything else in a rule is a variable, free to stand for anything.
	static bool isConstant(const QString& label);

	// THE CATEGORY THIS RULE IS ABOUT, ONE UNIVERSE DOWN - or nullptr.
	//
	// "Identity intro" is drawn as BigCat holding a category C holding an
	// object X, and it concludes that X has an identity. Used on a diagram
	// whose CANVAS is the category in question - objects drawn straight onto
	// BigCat - it found nothing, because the diagram has no category drawn
	// inside it for C to be.
	//
	// It should have. A canvas is a category, and although it is drawn in
	// nothing it is not nowhere: it sits in a universe one step up, and is an
	// object of the category of categories THERE. Not of itself - BigCat is
	// not an object of BigCat, and nobody should have to draw it as one to
	// get an identity arrow.
	//
	// So a rule of this shape has a second reading: its root is that implied
	// universe, and C is the canvas the rule is being used on. This returns
	// the C - the one category drawn directly in the rule's root - when the
	// rule can be read that way at all:
	//
	//   - the root holds exactly one thing, and that thing is a category;
	//   - no premise ARROW is drawn directly in the root (in the universe
	//     reading there is nothing drawn there for it to run between);
	//   - nothing the rule concludes or deletes is drawn directly in the
	//     root, because nothing may be made or unmade in a universe that is
	//     not drawn;
	//   - the root is a category of categories (BigCat, Cat) or a variable.
	//     A rule drawn in R-Mod is about R-Mod, not about a universe.
	Node* universeSubject() const;

private:
	void extract();
	// A term spelt out of something the rule only CLAIMS is part of what is
	// claimed, not part of what must be found: see the comment on the
	// definition. Run as the last step of extract().
	void promoteTermsOverExistentials();

	QString m_path;
	QString m_name;
	std::unique_ptr<DiagramScene> m_scene;
	Category* m_root = nullptr;
	QList<Node*> m_premiseObjects;
	QList<Arrow*> m_premiseArrows;
	QList<Node*> m_conclusionObjects;
	QList<Arrow*> m_conclusionArrows;
	QList<Node*> m_deletedObjects;
	QList<Arrow*> m_deletedArrows;
};

// One place the premise was found in a diagram: which node of the pattern
// stands for which node of the diagram. A monomorphism - two things in the
// pattern never stand for one thing in the diagram.
struct RuleMatch
{
	QHash<Node*, Node*> objects;    // pattern object (or root) -> diagram node
	QHash<Arrow*, Arrow*> arrows;   // pattern arrow -> diagram arrow
	QRectF bounds;                  // where it sits, in scene coordinates

	// the variable -> expression substitution the match amounts to
	QHash<QString, QString> bindings() const;
};

namespace RuleMatcher
{
	// Every place the rule's premise sits inside the diagram, as a subgraph
	// with the same nesting and the same arrows between the same things,
	// constants matching constants and variables standing for anything -
	// consistently. Capped, because a pattern of three loose objects in a
	// diagram of thirty has a great many embeddings.
	QList<RuleMatch> find(const Rule& rule, DiagramScene* diagram, int cap = 200);

	// Draw the conclusion into the diagram at that match, the variables
	// substituted. What is made is returned, and recorded as one step.
	QList<Node*> apply(const Rule& rule, const RuleMatch& match, DiagramScene* diagram);
}
