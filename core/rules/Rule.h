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

private:
	void extract();

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
