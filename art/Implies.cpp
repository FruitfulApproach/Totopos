#include "art/Implies.h"
#include "art/GraphicsHelpers.h"

#include <QPen>
#include <QMenu>

Implies::Implies(const QString& ruleName, Node* givens, Node* conclusion, QGraphicsItem* parent)
	: Arrow(ruleName, givens, conclusion, parent)   // Arrow has no default constructor
{
	// A heavier, darker line than a morphism's: this one is not part of the
	// mathematics being drawn, it is the joint between the two halves of the
	// statement, and it has to read as the outermost thing in the picture.
	setBorder(QPen(QColor(40, 44, 60), 2.0));
	setZValue(20);

	// Out of the mouse's way entirely, label and all. There is nothing to
	// click: an implication is not drawn by hand and cannot be edited, bent,
	// selected or deleted. It is there because the diagram says so, and it
	// goes when the classical view goes.
	setInert();
}

Implies::~Implies()
{
	safeRemoveAndLog(this, "Implies");
}

QString Implies::contextTitle() const
{
	return QStringLiteral("the implication");
}

void Implies::populateActions(QMenu& menu)
{
	Q_UNUSED(menu);
}
