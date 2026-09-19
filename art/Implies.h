#pragma once

#include "art/Arrow.h"

// A ==> B: "these givens imply this conclusion", with the rule's name written
// over the line.
//
// Not a morphism. Its two ends are not objects of a category but whole
// diagrams - the box of what is given and the box of what follows - and what
// runs between them is an implication, which is why it is drawn the way
// implication is written: a double line with one head.
//
// WHERE THIS COMES FROM. A diagram in this program is a rule already: solid is
// the premise, dotted is what the rule says there then is. That is the
// succinct notation, and it says the whole thing in one picture. The classical
// notation says the same thing in two pictures and an arrow between them, and
// an Implies is that arrow. Nothing about the diagram changes when you switch
// between them - see core/view/ClassicalView.h, which builds this view and
// takes it down again.
//
// An Implies is DRAWN, not drawn BY anyone: it is made by the classical view
// and goes when that view goes. It never reaches a file, never takes a click,
// and is never part of what the diagram claims.
class Implies : public Arrow
{
	Q_OBJECT

public:
	// `ruleName` is what the diagram is called - "Snake lemma", "Kernels
	// exist" - and rides the line. Empty is fine: an implication with no name
	// is still an implication.
	Implies(const QString& ruleName, Node* givens, Node* conclusion,
	        QGraphicsItem* parent = nullptr);
	~Implies() override;

	// "the implication"
	QString contextTitle() const override;

protected:
	// a double line, and a head on it: see Arrow::drawsDoubleLine
	bool drawsDoubleLine() const override { return true; }
	bool drawsHead() const override { return true; }
	// wider than an equals: these two lines have a head to carry between them
	qreal doubleLineGap() const override { return 3.0; }

	// Nothing to offer. Everything a normal arrow can be asked - what kind of
	// morphism it is, what it carries over, whether it is claimed to exist -
	// is meaningless here, and taking it away is not an omission.
	void populateActions(QMenu& menu) override;
};
