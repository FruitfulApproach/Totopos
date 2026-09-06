#include "HasProducts.h"

#include <QMenu>
#include "../Category.h"
#include "../Arrow.h"
#include "../DiagramScene.h"
#include "../TutorSession.h"

namespace
{
	QString subscript(int n)
	{
		QString s;
		for (QChar c : QString::number(n))
			s += QChar(0x2080 + (c.unicode() - '0'));
		return s;
	}

	QString factorList(const QList<Node*>& picks)
	{
		QStringList ids;
		for (Node* n : picks)
			ids << n->id();
		return ids.join(", ");
	}
}

void HasProducts::categoryContextMenu(QMenu& menu, Category* category)
{
	menu.addSection("Products");
	QAction* define = menu.addAction("Define a product...");
	QObject::connect(define, &QAction::triggered, this, [this, category] {
		if (auto* scene = dynamic_cast<DiagramScene*>(category->scene()))
			teach(scene);
	});
}

void HasProducts::onBegin(TutorSession& s)
{
	s.say("Select the objects that go INTO the product, in order: click each one. "
	      "Press Done (Enter) when they are all chosen; Esc cancels.", category());
}

bool HasProducts::onPick(TutorSession& s, Node* node)
{
	// a factor is an object of THIS category, not yet chosen
	if (node == nullptr || node->parentItem() != category() || dynamic_cast<Arrow*>(node) != nullptr)
	{
		s.say("That is not an object of " + category()->id() + ". Pick objects drawn inside it, in order.", category());
		return false;
	}
	if (s.picks().contains(node))
		return false;
	s.say("Factors so far: " + factorList(s.picks()) + (s.picks().isEmpty() ? "" : ", ") + node->id()
	      + ". Pick the next factor, or press Done.", node);
	return true;
}

bool HasProducts::onDone(TutorSession& s)
{
	const QList<Node*>& picks = s.picks();
	if (picks.size() < 2)
	{
		s.say("A product needs at least two factors: " + QString::number(picks.size()) + " chosen so far.", category());
		return false;
	}

	Category* cat = category();
	QStringList names;
	QPointF centroid;
	for (Node* n : picks)
	{
		names << n->id();
		centroid += n->pos();
	}
	centroid /= picks.size();

	// the product, below the factors, with one projection per factor — made
	// by the category, so a product of categories is a category
	Object* product = cat->createObject(names.join(QString(" ") + QChar(0x00D7) + " "), cat->mapToScene(centroid + QPointF(0, 110)));
	for (int i = 0; i < picks.size(); ++i)
	{
		auto* p = new Arrow("p" + subscript(i + 1), product, picks[i], cat);
		p->setZValue(1);
	}
	return true;
}
